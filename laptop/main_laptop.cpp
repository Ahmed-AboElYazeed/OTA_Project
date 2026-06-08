#include <CommonAPI/CommonAPI.hpp>
#include <v1/com/myapp/ota/OtaUpdateProxy.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <openssl/evp.h>

using namespace v1::com::myapp::ota;

static const std::string DOMAIN    = "local";
static const std::string INSTANCE  = "com.myapp.ota.OtaUpdate";
static const size_t      CHUNK_SIZE = 512 * 1024; // 512 KB per chunk

// ── SHA256 of a local file ─────────────────────────────────────────────────
std::string sha256File(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    std::vector<char> buf(1024 * 1024);
    while (f.read(buf.data(), buf.size()) || f.gcount() > 0)
        EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(f.gcount()));

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  len = 0;
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    return oss.str();
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <image_path> <new_version>\n";
        return 1;
    }

    const std::string imagePath  = argv[1];
    const std::string newVersion = argv[2];

    // ── Open image file ──────────────────────────────────────────────────
    std::ifstream imageFile(imagePath, std::ios::binary | std::ios::ate);
    if (!imageFile) {
        std::cerr << "Cannot open image: " << imagePath << "\n";
        return 1;
    }
    uint64_t imageSize = static_cast<uint64_t>(imageFile.tellg());
    imageFile.seekg(0);

    std::cout << "[CLIENT] Image: " << imagePath
              << "  size: " << imageSize << " bytes\n";

    // ── Compute SHA256 ───────────────────────────────────────────────────
    std::cout << "[CLIENT] Computing SHA256...\n";
    std::string hash = sha256File(imagePath);
    std::cout << "[CLIENT] SHA256: " << hash << "\n";

    // ── Build proxy ──────────────────────────────────────────────────────
    // buildProxy<> takes the template OtaUpdateProxy, NOT the typedef
    auto runtime = CommonAPI::Runtime::get();
    auto proxy   = runtime->buildProxy<OtaUpdateProxy>(DOMAIN, INSTANCE,
                                                        "OtaUpdateClient");

    std::cout << "[CLIENT] Waiting for OTA service on RPi...\n";
    while (!proxy->isAvailable())
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[CLIENT] Service available!\n";

    // ── Subscribe to status events ───────────────────────────────────────
    proxy->getUpdateStatusEvent().subscribe(
        [](const std::string &status, const uint32_t &pct,
           const std::string &msg) {
            std::cout << "[RPi STATUS] " << status
                      << " (" << pct << "%) " << msg << "\n";
        });

    // ── AnnounceUpdate ───────────────────────────────────────────────────
    CommonAPI::CallStatus cs;
    bool        accepted = false;
    std::string message;

    proxy->AnnounceUpdate(newVersion, imageSize, hash, cs, accepted, message);
    if (cs != CommonAPI::CallStatus::SUCCESS || !accepted) {
        std::cerr << "[CLIENT] AnnounceUpdate failed: " << message << "\n";
        return 1;
    }
    std::cout << "[CLIENT] RPi accepted update: " << message << "\n";

    // ── Stream chunks ────────────────────────────────────────────────────
    uint64_t offset = 0;
    std::vector<uint8_t> chunkBuf(CHUNK_SIZE);

    while (offset < imageSize) {
        size_t toRead = std::min(CHUNK_SIZE,
                                 static_cast<size_t>(imageSize - offset));
        imageFile.read(reinterpret_cast<char*>(chunkBuf.data()), toRead);
        size_t actualRead = static_cast<size_t>(imageFile.gcount());

        CommonAPI::ByteBuffer chunk(chunkBuf.begin(),
                                    chunkBuf.begin() + actualRead);

        int64_t nextOffset = 0;
        proxy->SendChunk(offset, chunk, cs, nextOffset);

        if (cs != CommonAPI::CallStatus::SUCCESS || nextOffset < 0) {
            std::cerr << "[CLIENT] SendChunk failed at offset "
                      << offset << "\n";
            return 1;
        }

        // Resume: if RPi asks us to resend, seek back
        if (static_cast<uint64_t>(nextOffset) != offset + actualRead) {
            std::cout << "[CLIENT] Resuming from offset " << nextOffset << "\n";
            imageFile.seekg(static_cast<std::streamoff>(nextOffset));
        }

        offset = static_cast<uint64_t>(nextOffset);

        double pct = static_cast<double>(offset) /
                     static_cast<double>(imageSize) * 100.0;
        std::cout << "\r[CLIENT] Sent " << offset << " / " << imageSize
                  << " bytes (" << std::fixed << std::setprecision(1)
                  << pct << "%)     " << std::flush;
    }
    std::cout << "\n[CLIENT] All chunks sent.\n";

    // ── FinalizeUpdate ───────────────────────────────────────────────────
    bool success = false;
    proxy->FinalizeUpdate(hash, cs, success, message);
    if (cs != CommonAPI::CallStatus::SUCCESS || !success) {
        std::cerr << "[CLIENT] FinalizeUpdate failed: " << message << "\n";
        return 1;
    }
    std::cout << "[CLIENT] FinalizeUpdate: " << message << "\n";
    std::cout << "[CLIENT] Done. RPi is rebooting into new slot.\n";

    return 0;
}
