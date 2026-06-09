#include "OtaUpdateServiceImpl.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <vector>

static const std::string STATUS_FILE  = "/mydata/update-status.json";
static const std::string CMDLINE_PATH = "/boot/cmdline.txt";
static const std::string BOOT_MOUNT   = "/boot";
static const std::string SLOT_A_DEV   = "/dev/mmcblk0p2";
static const std::string SLOT_B_DEV   = "/dev/mmcblk0p3";
static const std::string PENDING_FILE = "/mydata/boot_pending";

// ── ctor/dtor ──────────────────────────────────────────────────────────────
OtaUpdateServiceImpl::OtaUpdateServiceImpl()
    : statusFile_(STATUS_FILE)
    , cmdlinePath_(CMDLINE_PATH)
    , partitionFd_(-1)
    , expectedOffset_(0)
    , totalImageSize_(0)
{
    targetPartition_ = resolveInactiveSlot();
    std::cout << "[OTA] Inactive slot (write target): "
              << targetPartition_ << "\n";
}

OtaUpdateServiceImpl::~OtaUpdateServiceImpl() {
    closePartition();
}

// ── resolveInactiveSlot ────────────────────────────────────────────────────
std::string OtaUpdateServiceImpl::resolveInactiveSlot() {
    std::ifstream f(CMDLINE_PATH);
    if (!f) {
        std::cerr << "[OTA] Cannot read cmdline.txt — defaulting to slot B\n";
        return SLOT_B_DEV;
    }
    std::string line;
    std::getline(f, line);
    f.close();

    if (line.find(SLOT_A_DEV) != std::string::npos) {
        std::cout << "[OTA] Currently booted from slot A → will write to slot B\n";
        return SLOT_B_DEV;
    }
    if (line.find(SLOT_B_DEV) != std::string::npos) {
        std::cout << "[OTA] Currently booted from slot B → will write to slot A\n";
        return SLOT_A_DEV;
    }

    std::cerr << "[OTA] Cannot determine active slot from cmdline — defaulting to slot B\n";
    return SLOT_B_DEV;
}

// ── AnnounceUpdate ─────────────────────────────────────────────────────────
void OtaUpdateServiceImpl::AnnounceUpdate(
    const std::shared_ptr<CommonAPI::ClientId> _client,
    std::string  _newVersion,
    uint64_t     _imageSize,
    std::string  _sha256Hash,
    AnnounceUpdateReply_t _reply)
{
    (void)_client;
    std::cout << "[OTA] AnnounceUpdate: version=" << _newVersion
              << " size=" << _imageSize
              << " hash=" << _sha256Hash << "\n";

    if (partitionFd_ >= 0) {
        _reply(false, "Update already in progress");
        return;
    }

    // Re-resolve every announce in case we rebooted into a different slot
    targetPartition_ = resolveInactiveSlot();
    totalImageSize_  = _imageSize;
    expectedHash_    = _sha256Hash;
    expectedOffset_  = 0;
    newVersion_      = _newVersion;

    if (!openPartition()) {
        _reply(false, "Failed to open target partition: " +
               std::string(strerror(errno)));
        return;
    }

    updateStatusFile("in_progress");
    fireProgress("in_progress", 0, "Update accepted — ready for chunks");
    _reply(true, "Ready — writing to " + targetPartition_);
}

// ── SendChunk ──────────────────────────────────────────────────────────────
void OtaUpdateServiceImpl::SendChunk(
    const std::shared_ptr<CommonAPI::ClientId> _client,
    uint64_t              _offset,
    CommonAPI::ByteBuffer _data,
    SendChunkReply_t      _reply)
{
    (void)_client;

    if (partitionFd_ < 0) {
        std::cerr << "[OTA] SendChunk: partition not open\n";
        _reply(-1);
        return;
    }

    if (_offset != expectedOffset_) {
        std::cerr << "[OTA] Offset mismatch: expected=" << expectedOffset_
                  << " got=" << _offset << "\n";
        _reply(static_cast<int64_t>(expectedOffset_));
        return;
    }

    if (lseek64(partitionFd_, static_cast<off64_t>(_offset), SEEK_SET) < 0) {
        std::cerr << "[OTA] lseek failed: " << strerror(errno) << "\n";
        _reply(-1);
        return;
    }

    ssize_t written = write(partitionFd_, _data.data(), _data.size());
    if (written != static_cast<ssize_t>(_data.size())) {
        std::cerr << "[OTA] write failed: " << strerror(errno) << "\n";
        _reply(-1);
        return;
    }

    expectedOffset_ += static_cast<uint64_t>(written);

    if (totalImageSize_ > 0) {
        uint32_t pct = static_cast<uint32_t>(
            (expectedOffset_ * 100) / totalImageSize_);
        static uint32_t lastPct = 0;
        if (pct != lastPct) {
            lastPct = pct;
            fireProgress("in_progress", pct,
                "Received " + std::to_string(expectedOffset_) +
                " / "       + std::to_string(totalImageSize_) + " bytes");
        }
    }

    _reply(static_cast<int64_t>(expectedOffset_));
}

// ── FinalizeUpdate ─────────────────────────────────────────────────────────
//
// Correct sequence:
//   1. fsync + close partition
//   2. SHA256 verify — FAIL → reply false, no reboot, no slot switch
//   3. switchBootSlot — FAIL → reply false, no reboot
//   4. updateStatusFile("complete")
//   5. write pending flag  ← AFTER slot switch confirmed
//   6. reply(true)         ← reply BEFORE reboot so QNX gets it
//   7. sleep 5s → reboot   ← detached, runs after reply is sent
//
void OtaUpdateServiceImpl::FinalizeUpdate(
    const std::shared_ptr<CommonAPI::ClientId> _client,
    std::string _sha256Hash,
    FinalizeUpdateReply_t _reply)
{
    (void)_client;
    std::cout << "[OTA] FinalizeUpdate: starting...\n";

    // ── Step 1: flush and close partition ─────────────────────────────────
    if (partitionFd_ < 0) {
        std::cerr << "[OTA] FinalizeUpdate: partition not open\n";
        updateStatusFile("failed");
        _reply(false, "Partition not open — was AnnounceUpdate called?");
        return;
    }

    fsync(partitionFd_);
    closePartition();
    std::cout << "[OTA] Partition flushed and closed\n";

    // ── Step 2: SHA256 verification ───────────────────────────────────────
    std::cout << "[OTA] Computing SHA256 of written partition ("
              << totalImageSize_ / (1024*1024) << " MB)...\n";

    std::string computed = computeSHA256();

    std::cout << "[OTA] Expected : " << _sha256Hash << "\n";
    std::cout << "[OTA] Computed : " << computed    << "\n";

    if (computed.empty()) {
        std::cerr << "[OTA] SHA256 computation failed (read error)\n";
        updateStatusFile("failed");
        fireProgress("failed", 0, "SHA256 computation error");
        _reply(false, "SHA256 computation failed");
        return;   // ← no reboot, no slot switch
    }

    if (computed != _sha256Hash) {
        std::cerr << "[OTA] SHA256 MISMATCH — aborting, keeping current slot\n";
        updateStatusFile("failed");
        fireProgress("failed", 0, "SHA256 mismatch — update aborted");
        _reply(false, "SHA256 mismatch: computed=" + computed);
        return;   // ← no reboot, no slot switch, system stays on current slot
    }
    std::cout << "[OTA] SHA256 OK\n";

    // ── Step 3: switch boot slot ──────────────────────────────────────────
    std::cout << "[OTA] Switching boot slot...\n";
    if (!switchBootSlot()) {
        std::cerr << "[OTA] Boot slot switch failed — keeping current slot\n";
        updateStatusFile("failed");
        fireProgress("failed", 0, "Boot slot switch failed");
        _reply(false, "Failed to switch boot slot");
        return;   // ← no reboot, system stays on current slot
    }
    std::cout << "[OTA] Boot slot switched successfully\n";

    // ── Step 4: update status file ────────────────────────────────────────
    updateStatusFile("complete", newVersion_);

    // ── Step 5: write pending flag ────────────────────────────────────────
    // This flag is ONLY written after slot switch is confirmed.
    // On next boot, boot-confirm.sh reads this flag and verifies
    // the new rootfs is healthy. If not, it switches back.
    {
        std::ofstream pf(PENDING_FILE, std::ios::trunc);
        if (pf) {
            pf << newVersion_ << "\n";
            pf.close();
            std::cout << "[OTA] Pending boot flag written\n";
        } else {
            std::cerr << "[OTA] Warning: could not write pending flag\n";
            // Non-fatal — continue with reboot
        }
    }

    // ── Step 6: reply to QNX BEFORE starting reboot timer ─────────────────
    // Critical: reply must be sent first so QNX receives SUCCESS.
    // The reboot runs in a detached thread 5 seconds later.
    fireProgress("complete", 100, "Update complete — rebooting in 5s");
    _reply(true, "Update verified and applied — rebooting in 5s");
    std::cout << "[OTA] Reply sent to QNX\n";

    // ── Step 7: reboot after delay ────────────────────────────────────────
    // Detached thread gives the TCP stack time to flush the reply.
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "[OTA] Rebooting now.\n";
        (void)::system("reboot");
    }).detach();
}

// ── openPartition ──────────────────────────────────────────────────────────
bool OtaUpdateServiceImpl::openPartition() {
    partitionFd_ = open(targetPartition_.c_str(), O_WRONLY | O_SYNC);
    if (partitionFd_ < 0) {
        std::cerr << "[OTA] Cannot open " << targetPartition_
                  << ": " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

// ── closePartition ─────────────────────────────────────────────────────────
void OtaUpdateServiceImpl::closePartition() {
    if (partitionFd_ >= 0) {
        close(partitionFd_);
        partitionFd_ = -1;
    }
}

// ── computeSHA256 ──────────────────────────────────────────────────────────
std::string OtaUpdateServiceImpl::computeSHA256() {
    int fd = open(targetPartition_.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[OTA] computeSHA256: cannot open "
                  << targetPartition_ << ": " << strerror(errno) << "\n";
        return "";
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    std::vector<uint8_t> buf(1024 * 1024); // 1 MB read buffer
    uint64_t remaining = totalImageSize_;

    while (remaining > 0) {
        size_t  toRead = std::min(remaining, (uint64_t)buf.size());
        ssize_t n      = read(fd, buf.data(), toRead);
        if (n <= 0) {
            std::cerr << "[OTA] computeSHA256: read error at remaining="
                      << remaining << "\n";
            break;
        }
        EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(n));
        remaining -= static_cast<uint64_t>(n);
    }
    close(fd);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digestLen = 0;
    EVP_DigestFinal_ex(ctx, digest, &digestLen);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < digestLen; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    return oss.str();
}

// ── switchBootSlot ─────────────────────────────────────────────────────────
bool OtaUpdateServiceImpl::switchBootSlot() {
    // Remount /boot read-write
    if (::mount(nullptr, BOOT_MOUNT.c_str(), nullptr,
                MS_REMOUNT | MS_NOATIME, nullptr) != 0) {
        std::cerr << "[OTA] remount /boot rw failed: "
                  << strerror(errno) << "\n";
        return false;
    }

    // Read current cmdline.txt
    std::ifstream in(cmdlinePath_);
    if (!in) {
        std::cerr << "[OTA] Cannot read " << cmdlinePath_ << "\n";
        ::mount(nullptr, BOOT_MOUNT.c_str(), nullptr,
                MS_REMOUNT | MS_RDONLY, nullptr);
        return false;
    }
    std::string line;
    std::getline(in, line);
    in.close();

    // Replace the root= device
    auto replaceInLine = [&](const std::string &from, const std::string &to) {
        size_t pos = line.find(from);
        if (pos != std::string::npos)
            line.replace(pos, from.size(), to);
    };

    if (line.find(SLOT_A_DEV) != std::string::npos) {
        replaceInLine(SLOT_A_DEV, SLOT_B_DEV);
        std::cout << "[OTA] cmdline.txt: slot A → B\n";
    } else if (line.find(SLOT_B_DEV) != std::string::npos) {
        replaceInLine(SLOT_B_DEV, SLOT_A_DEV);
        std::cout << "[OTA] cmdline.txt: slot B → A\n";
    } else {
        std::cerr << "[OTA] No known root= device in cmdline.txt: "
                  << line << "\n";
        ::mount(nullptr, BOOT_MOUNT.c_str(), nullptr,
                MS_REMOUNT | MS_RDONLY, nullptr);
        return false;
    }

    // Write back
    std::ofstream out(cmdlinePath_, std::ios::trunc);
    if (!out) {
        std::cerr << "[OTA] Cannot write " << cmdlinePath_ << "\n";
        ::mount(nullptr, BOOT_MOUNT.c_str(), nullptr,
                MS_REMOUNT | MS_RDONLY, nullptr);
        return false;
    }
    out << line << "\n";
    out.close();

    // Flush boot partition before remounting ro
    sync();
    ::mount(nullptr, BOOT_MOUNT.c_str(), nullptr,
            MS_REMOUNT | MS_RDONLY, nullptr);

    return true;
}

// ── updateStatusFile ───────────────────────────────────────────────────────
void OtaUpdateServiceImpl::updateStatusFile(const std::string &status,
                                            const std::string &version) {
    std::ifstream in(statusFile_);
    if (!in) {
        std::cerr << "[OTA] Cannot open status file: " << statusFile_ << "\n";
        return;
    }

    // Skip any comment lines (lines starting with //)
    std::string content;
    std::string line;
    while (std::getline(in, line)) {
        if (line.substr(0, 2) == "//") continue; // strip comments
        content += line + "\n";
    }
    in.close();

    auto replaceVal = [&](const std::string &key, const std::string &val) {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return;
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) return;
        size_t q1 = content.find('"', colon);
        size_t q2 = content.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) return;
        content.replace(q1, q2 - q1 + 1, "\"" + val + "\"");
    };

    replaceVal("status", status);

    if (status == "complete") {
        bool wroteSlotB = (targetPartition_ == SLOT_B_DEV);
        if (!version.empty())
            replaceVal(wroteSlotB ? "version_b" : "version_a", version);
        replaceVal("active_slot", wroteSlotB ? "b" : "a");
    }

    // Write back without the comment line
    std::ofstream out(statusFile_, std::ios::trunc);
    if (out) out << content;
}

// ── fireProgress ───────────────────────────────────────────────────────────
void OtaUpdateServiceImpl::fireProgress(const std::string &status,
                                        uint32_t percent,
                                        const std::string &msg) {
    fireUpdateStatusEvent(status, percent, msg);
}