#pragma once
#include <string>

static const std::string b64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string encryptData(const std::string& data, const std::string& password);
std::string decryptData(const std::string& data, const std::string& password);
std::string base64_encode(const std::string& in);
std::string base64_decode(const std::string& in);
