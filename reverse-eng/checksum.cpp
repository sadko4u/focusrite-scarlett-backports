#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, const char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "USAGE: checksum <in-file> <start-hex> <end-hex>\n");
        return -1;
    }

    long from = strtol(argv[2], NULL, 16);
    long to   = strtol(argv[3], NULL, 16);
    if ((to < from) || (to % sizeof(uint32_t)) || (from % sizeof(uint32_t)))
    {
        fprintf(stderr, "Bad address range: %lx to %lx\n", from, to);
        return -1;
    }

    FILE *fd = fopen(argv[1], "r");
    if (fd == NULL)
    {
        fprintf(stderr, "Could not open for reading: %s\n", argv[1]);
        return -1;
    }

    uint32_t *data = reinterpret_cast<uint32_t *>(malloc(to - from));
    size_t count = (to - from) / sizeof(uint32_t);

    fseek(fd, from, SEEK_SET);
    fread(data, to - from, 1, fd);

    uint32_t cksum = 0;
    for (size_t i=0; i<count; ++i)
        cksum      += data[i];

    printf("ROM checksum: 0x%08x\n", (int)cksum);
    printf("Inverse checksum: 0x%08x\n", (int)((~cksum) + 1));

    free(data);
    fclose(fd);

    return 0;
}
