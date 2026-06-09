#pragma once
#include <v1/com/myapp/ota/OtaUpdateStubDefault.hpp>
#include <string>
#include <fstream>
#include <thread>

class OtaUpdateServiceImpl : public v1::com::myapp::ota::OtaUpdateStubDefault {
public:
    OtaUpdateServiceImpl();
    ~OtaUpdateServiceImpl();

    // Called by laptop to announce a new update is available
    void AnnounceUpdate(
        const std::shared_ptr<CommonAPI::ClientId> _client,
        std::string  _newVersion,
        uint64_t     _imageSize,
        std::string  _sha256Hash,
        AnnounceUpdateReply_t _reply) override;

    // Called repeatedly by laptop — one chunk per call
    void SendChunk(
        const std::shared_ptr<CommonAPI::ClientId> _client,
        uint64_t              _offset,
        CommonAPI::ByteBuffer _data,
        SendChunkReply_t      _reply) override;

    // Called by laptop after last chunk — verify, switch slot, reboot
    void FinalizeUpdate(
        const std::shared_ptr<CommonAPI::ClientId> _client,
        std::string _sha256Hash,
        FinalizeUpdateReply_t _reply) override;

private:
    // Partition and state
    std::string   targetPartition_;   // /dev/mmcblk0p3 (or p2)
    std::string   statusFile_;        // /mydata/update-status.json
    int           partitionFd_;       // open fd to target partition
    uint64_t      expectedOffset_;    // next expected byte offset
    uint64_t      totalImageSize_;    // set at AnnounceUpdate
    std::string   expectedHash_;      // SHA256 from AnnounceUpdate
    std::string   newVersion_;        // version string from AnnounceUpdate
    std::string   activeSlot_;        // current active slot (a or b)

    // Helpers — U-Boot environment management
    std::string   resolveInactiveSlot();     // Read active_slot via fw_printenv, return inactive slot
    std::string   getActiveSlot() const;     // Call fw_printenv to read active_slot
    bool          setActiveSlot(const std::string &slot);  // Call fw_setenv to set active_slot
    bool          openPartition();
    void          closePartition();
    std::string   computeSHA256();
    bool          switchBootSlot();          // Now uses U-Boot environment (fw_setenv)
    void          updateStatusFile(const std::string &status,
                                   const std::string &version = "");
    void          fireProgress(const std::string &status,
                               uint32_t percent,
                               const std::string &msg);
};
