#include "stdio.h"
#include "getopt.h"
#include "stdlib.h"
#include "elf.h"
#include <map>
#include <string>

std::map<int, std::string> stt_name;
std::map<int, std::string> stb_name;
std::map<int, std::string> sh_name;

static void init()
{
  sh_name[SHT_NULL] = "NULL";
  sh_name[SHT_SYMTAB] = "SYMTAB";
  sh_name[SHT_STRTAB] = "STRTAB";
  sh_name[SHT_RELA] = "RELA";
  sh_name[SHT_PROGBITS] = "PROGBITS";
  sh_name[SHT_NOBITS] = "NOBITS";
  sh_name[SHT_NOTE] = "NOTE";

  stt_name[STT_OBJECT] = "OBJECT";
  stt_name[STT_FUNC] = "FUNC";
  stt_name[STT_SECTION] = "SECTION";
  stt_name[STT_FILE] = "FILE";

  stb_name[STB_LOCAL] = "LOCAL";
  stb_name[STB_GLOBAL] = "GLOBAL";
  stb_name[STB_WEAK] = "WEAK";

}

static bool rdelf(FILE* file, void* ptr, size_t len)
{
  size_t size = fread(ptr, len, 1, file);
  if (size != 1) {
    if (feof(file)) {
      printf("EOF detected on file\n");
    } else if (ferror(file)) {
      printf("error on fread(%ld)\n", size);
    } else {
      printf("unknown condition on fread\n");
    }
    return false;
  }
  return true;
}

static bool check_ident(unsigned char ident[EI_NIDENT])
{
  return (ident[EI_MAG0] == ELFMAG0 &&
	  ident[EI_MAG1] == ELFMAG1 &&
	  ident[EI_MAG2] == ELFMAG2 &&
	  ident[EI_MAG3] == ELFMAG3);
}

static int process_file(const char* filename)
{
  FILE* file = fopen(filename, "r");
  if (file == nullptr) {
    printf("Unable to open file\n");
    return -1;
  }

  Elf64_Ehdr ehdr;
  if (!rdelf(file, &ehdr, sizeof(ehdr)))
    return -1;

  if (!check_ident(ehdr.e_ident))
      return -1;
  if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
    printf("only support ELF 64 for now\n");
    return -1;
  }
  if (ehdr.e_type != ET_REL) {
    printf("Not relocatable object\n");
    return -1;
  }
  if (ehdr.e_machine != EM_X86_64) {
    printf("Not AMD x86-64 arch\n");
    return -1;
  }

  if (ehdr.e_shoff) {
    if (fseek(file, ehdr.e_shoff, SEEK_SET) != 0) {
      printf("Unable to seek to offset e_shoff=%ld\n", ehdr.e_shoff);
      return -1;
    }
    Elf64_Shdr shdr[ehdr.e_shnum];
    if (sizeof(Elf64_Shdr) != ehdr.e_shentsize) {
      printf("Error section header entry size incorrect\n");
      return -1;
    }
    if (fread(&shdr, ehdr.e_shentsize, ehdr.e_shnum, file) != ehdr.e_shnum) {
      printf("Failed to read all %d section headers\n", ehdr.e_shnum);
      return -1;
    }
    Elf64_Shdr *symtab, *strtab;
    for (int i=0; i<ehdr.e_shnum; i++) {
      switch (shdr[i].sh_type) {
      case SHT_SYMTAB:
	{
	  symtab = &shdr[i];
	  printf("i=%d type=%d\n", i, symtab->sh_type);
	  break;
	}
      case SHT_STRTAB:
	{
	  strtab = &shdr[i];
	  printf("i=%d type=%d\n", i, strtab->sh_type);
	  fseek(file, strtab->sh_offset, SEEK_SET);
	  char buffer[strtab->sh_size+1];
	  fread(buffer, strtab->sh_size, 1, file);
	  break;
	}
      default:
	break;
      }
      // auto st_type = ELF64_ST_TYPE(shdr.sh_info);
      // auto st_bind = ELF64_ST_BIND(shdr.sh_info);
      // printf("type=%d bind=%d\n", st_type, st_bind);
      // printf("type=%s bind=%s\n", stt_name[st_type].data(), stb_name[st_bind].data());
    }
  }

  return 0;
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    printf("usage: %s FILENAME\n", argv[0]);
    exit(-1);
  }

  init();
  
  if (process_file(argv[1]))
    exit(1);

  exit(0);
}
