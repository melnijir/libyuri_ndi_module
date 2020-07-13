/*
 * Licence.h
  */
#ifndef LICENCE_H_
#define LICENCE_H_

/* General */
#include <stdexcept>
#include <stdio.h>      
#include <string>
#include <sstream> 
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <ctime> 

/* Net */
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>

/* SSL */
#include <openssl/evp.h>
#include <openssl/pem.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

namespace yuri {
namespace ndi {

typedef unsigned char byte;

const size_t limited_outputs = 2;
const size_t limited_streams = 4;
const size_t limited_resolution = 1080;

struct LicenceCaps {
    size_t version;
    std::string name;
    std::string date;
    size_t streams;
    size_t streams_given;
    size_t outputs;
    size_t resolution;
    std::string level;
};

struct LicenceData {
    LicenceCaps caps;
    std::string ifname;
    std::string mac_address;
};

class Licence {
    public:
        static Licence& getInstance() {
            static Licence instance;
            return instance;
        }

        void set_licence_file(std::string path);
        LicenceCaps get_licence();
        void return_licence();

    private:
        Licence() {}

        std::string get_mac_addr(std::string ifname);
        LicenceData get_license_from_file(std::string filename);

        size_t given_ = 0;
        std::string licence_path_;
        std::mutex licence_lock_;
        LicenceData licence_data_;

    public:
        Licence(Licence const&)         = delete;
        void operator=(Licence const&)  = delete;
};

}

}

#endif /* LICENCE_H_ */
