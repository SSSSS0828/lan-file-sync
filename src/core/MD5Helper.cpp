#include "MD5Helper.h"
#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <memory>

static std::string digestToHex(const unsigned char* digest, unsigned int len)
{
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    return oss.str();
}

std::string MD5Helper::compute(const void* data, size_t len)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  dlen = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, digest, &dlen);
    EVP_MD_CTX_free(ctx);

    return digestToHex(digest, dlen);
}

std::string MD5Helper::computeFile(const std::string& filepath)
{
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) throw std::runtime_error("Cannot open file: " + filepath);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);

    const size_t BUF = 4 * 1024 * 1024;
    std::vector<char> buf(BUF);
    while (ifs.read(buf.data(), static_cast<std::streamsize>(BUF)) ||
           ifs.gcount() > 0)
    {
        EVP_DigestUpdate(ctx, buf.data(),
                         static_cast<size_t>(ifs.gcount()));
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  dlen = 0;
    EVP_DigestFinal_ex(ctx, digest, &dlen);
    EVP_MD_CTX_free(ctx);

    return digestToHex(digest, dlen);
}
