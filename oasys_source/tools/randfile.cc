#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <cerrno>
#include <fcntl.h>

#include "../debug/Log.h"
#include "../io/FileUtils.h"
#include "../util/App.h"
#include "../util/Random.h"
#include "../util/StringUtils.h"

using namespace oasys;


class RandFile : public App {
public:
    RandFile() : App("RandFile", "randfile") {}
    void fill_options();
    int main(int argc, char* argv[]);

protected:
    u_int64_t   length_;
    bool        ascii_;
    std::string output_;
};

void
RandFile::fill_options()
{
    length_ = 0;
    ascii_  = false;
    output_ = "-";
    
    fill_default_options(0);
    opts_.addopt(new SizeOpt('L', "length", &length_));
    opts_.addopt(new BoolOpt('A', "ascii", &ascii_));
    opts_.addopt(new StringOpt('O', "output", &output_));
}

int
RandFile::main(int argc, char* argv[])
{
    loglevel_ = LOG_WARN;
    logfile_  = "--"; // stderr
    
    init_app(argc, argv);
    
    int fd;
    if (output_ == "-") {
        fd = 1;
    } else {
        fd = open(output_.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fd < 1) {
            log_err_p("/randfile", "error opening output file '%s': %s",
                      output_.c_str(), strerror(errno));
            exit(1);
        }
    }
    
    ByteGenerator g(random_seed_);

    char buf[512];
    std::string hex;

    while (length_ > 0) {
        g.fill_bytes(buf, 512);

        const char* output;
        u_int64_t   output_len;

        if (ascii_) {
            hex2str(&hex, buf, sizeof(buf));
            output = hex.data();
            output_len = hex.length();
        } else {
            output = buf;
            output_len = sizeof(buf);
        }

        output_len = std::min(output_len, length_);

        if (::write(fd, output, output_len) != (int)output_len) {
            fprintf(stderr, "error writing output: %s", strerror(errno));
            exit(1);
        }

        length_ -= output_len;
    }

    return 0;
}
    
int
main(int argc, char* argv[])
{
    RandFile r;
    return r.main(argc, argv);
}
           
