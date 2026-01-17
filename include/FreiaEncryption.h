#pragma once
#include <string>
#include <array>

namespace FreiaEncryption
{
    static const std::string b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    using Key = std::array<unsigned char, 32>;

    std::string encryptData(const std::string& data, const Key& key);
    std::string decryptData(const std::string& data, const Key& key);
    std::string base64_encode(const std::string& in);
    std::string base64_decode(const std::string& in);
    Key deriveKey(const std::string& password);

}
