#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <cerrno>

#include "../debug/Log.h"
#include "../io/FileUtils.h"
#include "../io/MmapFile.h"
#include "../util/MD5.h"

using namespace oasys;

int
main(int argc, const char** argv)
{
    const char* LOG = "/md5chunks";
    
    if (argc != 3) {
        fprintf(stderr, "usage: %s <filename> <size>\n", argv[0]);
        exit(1);
    }

    const char* filename = argv[1];
    size_t chunk_size = (size_t)atoi(argv[2]);

    Log::init();

    int size = FileUtils::size(filename);
    if (size < 0) {
        log_err_p(LOG, "error getting file size %s: %s",
                  filename, strerror(errno));
        exit(1);
    }

    MmapFile mm("/md5chunks/mmap");
    const u_char* bp = (const u_char*)mm.map(filename, PROT_READ, 0);
    if (bp == NULL) {
        log_err_p(LOG, "error mmap'ing file: %s", strerror(errno));
        exit(1);
    }
    
    size_t todo = size;
    int i = 0;
    do {
        size_t chunk = std::min(chunk_size, todo);
        MD5 md5;
        md5.update(bp, chunk);
        md5.finalize();
        printf("%s\t%d\t%zu\t%s\n", filename, i, chunk, md5.digest_ascii().c_str());
        fflush(stdout);
        i++;
        todo -= chunk;
        bp   += chunk;
    } while (todo != 0);
}
