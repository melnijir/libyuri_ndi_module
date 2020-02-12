#include "Licence.h"

namespace yuri {
namespace ndi {

namespace {

const size_t digest_len = 512;
const size_t code_len = 24;

constexpr char const *code[] = {
    "2=2=2=2=2=424547494>205055424<4943204;",
    "45592=2=2=2=2=0:4=494943496:414>42676;",
    "71686;",
    "6947397730424151454641414?43416738414=49494343674;",
    "434167454173524?324<444>424477726:54356857456=6;",
    "470:3668464>4945322?79427:5264637:46553942724:436>4;",
    "3770576479715467734265464?427849775346326?6531687352696<5952516<7670757:2?73346>0:4738567:77695636306666716;",
    "724<525776437266555842565:46547478537073555752794>4=384:6?43545732335055366;",
    "4324487641:621>4968;",
    "473752316872394:483538626;0:2?48722;",
    "343863444342634<3979576<627934494?4138496<31706>726467616935437876445377436:51754=424<436;",
    "5445754<3663526:69776>32336=680:6<4>595:47365477306172377770766534656=75492?4870565:58626:745:586837326>5:7137775672446749387954663652743635413379417:47384937730:6948523738635075736335574965656=2?6>2;",
    "4:4>644?66342;",
    "6134563754625237487:52334:6756664<6<6278535271594=56777:2?6663645765554530470:5879527446656?3877614:4:7636585973533864317:7444525764784<596:5542414>6:394<5751584?3233704?4>37723546304;",
    "317962564?67446<2?506>0:77534444455848764=46444:66366250496?456468784>4>506150674735755462475264746?62594>626:4=4=5479482;5362724=71527976705532316=4;",
    "520:342;",
    "776?575:4;",
    "568504<3546614<7846474?4?0;",
    "41724?736<314866416?6644673255394?6:5231684=5543645871414>7:4>51365079345043624462744?705568504<3546614<7846474?4?0:624=6171374=446>78456=6<38797:6=616667485437414>673266515:654?317747416754764<35652?4;",
    "666?78686439652;2?7769565850735265783937780:4>61364>4?68544735652?5049416>2?46334?62384755466?6<72716;2?7:636=3356392;676863317046795964705335664<2;",
    "41637:35366<4=3245556=6:0:64664>53364636637:3661317:676?39672;",
    "656943536;",
    "434177454141513=3=0:2=2=2=2=2=454>44205055424<4943204;",
    "45592=2=2=2=2=0:"
};

constexpr char strmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', ':', ';', '<', '=', '>', '?'};

std::vector<std::string> split(const std::string& input, char delimiter) {
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream input_s(input);
   while (std::getline(input_s, token, delimiter)) {
      tokens.push_back(token);
   }
   return tokens;
}

std::vector<byte> str_to_bin(const std::string text) {
    std::vector<byte> data(text.length() / 2);
    for (unsigned int i = 0; i < text.length() / 2; ++i) {
        data[i] = ((text[2 * i] - 48) << 4) | (text[2 * i + 1] - 48);
    }
    return data;
}

bool verify(const std::vector<byte> signed_content, const std::vector<byte> original_content) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    if (!ctx) {
        return false;
    }
    
    std::string cs;
    for (size_t i = 0; i < code_len; i++) {
        if (i != 8 && i != 17) cs += code[i];
    }
    std::vector<byte> vc = str_to_bin(cs);
    BIO *bufio = BIO_new_mem_buf((void*)&vc[0], 800);
    EVP_PKEY *key = PEM_read_bio_PUBKEY(bufio, NULL, NULL, NULL);
    BIO_free_all(bufio);
    if (!key) {
        EVP_MD_CTX_destroy(ctx);
        return false;
    }
    if(1 != EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, key)) {
        EVP_MD_CTX_destroy(ctx);
        return false;
    }

    if(1 != EVP_DigestVerifyUpdate(ctx, original_content.data(), original_content.size())) {
        EVP_MD_CTX_destroy(ctx);
        return false;
    }

    if(1 == EVP_DigestVerifyFinal(ctx, signed_content.data(), signed_content.size())) {
        EVP_MD_CTX_destroy(ctx);
        return true;
    } else {
        EVP_MD_CTX_destroy(ctx);
        return false;
    }
}

}

void Licence::set_licence_file(const std::string path) {
    std::lock_guard<std::mutex> guard(licence_lock_);
    if (licence_path_.empty()) {
        licence_path_ = path;
        licence_data_ = get_license_from_file(licence_path_);
    }
}

LicenceCaps Licence::get_licence() {
    if (++given_ <= licence_data_.caps.streams) {
        // Check interface
        if (licence_data_.ifname != "any" && licence_data_.mac_address != get_mac_addr(licence_data_.ifname))
            throw std::out_of_range("Licence is not valid!");
        // Check date
        std::tm licence_date = {};
        std::istringstream time_s(licence_data_.caps.date);
        time_s >> std::get_time(&licence_date,"%Y-%m-%d");
        auto tpl = std::chrono::system_clock::from_time_t(std::mktime(&licence_date));
        auto tpn = std::chrono::system_clock::now();
        if (licence_data_.caps.date != "0000-00-00" && (tpl < tpn))
            throw std::out_of_range("Licence is not valid!");
        return licence_data_.caps;
    } else {
        throw std::out_of_range("No more licences availible. Turn off some streams.");
    }
}

void Licence::return_licence() {
    given_--;
}

std::string Licence::get_mac_addr(std::string ifname) {
    struct ifaddrs * if_addr_list_;
    getifaddrs(&if_addr_list_);

    for (struct ifaddrs * if_addr = if_addr_list_; if_addr != nullptr; if_addr = if_addr->ifa_next) {
        if (!if_addr->ifa_addr)
            continue;
        if (if_addr->ifa_addr->sa_family == AF_PACKET) {
			struct sockaddr_ll * saddr = (struct sockaddr_ll *)(if_addr->ifa_addr);
			std::stringstream sstream;
			for (int i = 0; i < 6; i++)
				sstream << std::setfill('0') << std::setw(2) << std::hex << (uint)saddr->sll_addr[i] << (i < 5 ? ":" : "");
			if (!strcmp(ifname.c_str(), if_addr->ifa_name)) {
				return sstream.str();
			}
		}
    }
    return "";
}

LicenceData Licence::get_license_from_file(std::string filename) {
    LicenceData ldata;

    // Read licence file
	std::ifstream ifile(filename, std::ios::binary | std::ios::ate);
	if (!ifile)
		throw std::runtime_error("Could not read licence file " + filename + "!");
	std::streamsize isize = ifile.tellg();
	ifile.seekg(0, std::ios::beg);
	std::vector<byte> ibuffer(isize);
	if (!ifile.read((char*)ibuffer.data(), isize))
        throw std::runtime_error("Could not read licence file " + filename + "!");
	ifile.close();


    // Convert it
    std::vector<byte> digets(digest_len);
    std::copy(ibuffer.begin(), ibuffer.begin()+(digest_len/2), digets.begin());
    std::copy(ibuffer.end()-(digest_len/2), ibuffer.end(), digets.begin()+(digest_len/2));
    std::vector<byte> licence(isize-digest_len);
    std::copy(ibuffer.begin()+(digest_len/2), ibuffer.end()-(digest_len/2), licence.begin());
    licence.push_back(0);
    std::vector<byte> licence_dec = str_to_bin((char*)licence.data());

    // Valid it
    if (verify(digets, licence_dec)) {
        licence_dec.push_back(0);
        auto lic_parts = split((char*)licence_dec.data(), ';');
        if (lic_parts.size() < 9)
            throw std::runtime_error("Wrong licence format!");
        ldata.caps.version = std::stoi(lic_parts.at(0));
        ldata.caps.name       = lic_parts.at(1);
        ldata.ifname          = lic_parts.at(2);
        ldata.mac_address     = lic_parts.at(3);
        ldata.caps.date       = lic_parts.at(4);
        ldata.caps.streams    = std::stoi(lic_parts.at(5));
        ldata.caps.outputs    = std::stoi(lic_parts.at(6));
        ldata.caps.resolution = std::stoi(lic_parts.at(7));
        ldata.caps.level      = lic_parts.at(8);
    } else {
        throw std::runtime_error("Licence is not valid!");
    }

    return ldata;
}



}
}