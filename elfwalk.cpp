#include "stdio.h"
#include "getopt.h"
#include "stdlib.h"
#include "elf.h"
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

std::map<int, std::string> stt_name;
std::map<int, std::string> stb_name;
std::map<int, std::string> sht_name;
std::map<int, std::string> strtab;
std::map<int, std::string> shstrtab;
std::map<int, std::string> symtab;
std::map<std::string, int> symtab_lookup;

static void init()
{
  sht_name[SHT_NULL]      = "NULL";
  sht_name[SHT_SYMTAB]    = "SYMTAB";
  sht_name[SHT_STRTAB]    = "STRTAB";
  sht_name[SHT_RELA]      = "RELA";
  sht_name[SHT_PROGBITS]  = "PROGBITS";
  sht_name[SHT_NOBITS]    = "NOBITS";
  sht_name[SHT_NOTE]      = "NOTE";

  stt_name[STT_NOTYPE]    = "NOTYPE";
  stt_name[STT_OBJECT]    = "OBJECT";
  stt_name[STT_FUNC]      = "FUNC";
  stt_name[STT_SECTION]   = "SECTION";
  stt_name[STT_FILE]      = "FILE";

  stb_name[STB_LOCAL]     = "LOCAL";
  stb_name[STB_GLOBAL]    = "GLOBAL";
  stb_name[STB_WEAK]      = "WEAK";
}

static bool check_ident(unsigned char ident[EI_NIDENT])
{
  return (ident[EI_MAG0] == ELFMAG0 &&
	  ident[EI_MAG1] == ELFMAG1 &&
	  ident[EI_MAG2] == ELFMAG2 &&
	  ident[EI_MAG3] == ELFMAG3);
}

Elf64_Sym* find_symbol_entry(char* buffer, std::string& name)
{
  int idx = symtab_lookup[name];
  if (idx == 0)
    return nullptr;

  return (Elf64_Sym*)(buffer + (idx * sizeof(Elf64_Sym)));
}

bool parse_symtab(char* buffer, long size, long entsize)
{
  printf("[%s]\n", __func__);
  if (entsize != sizeof(Elf64_Sym)) {
    printf("symtab entry invalid size\n");
    return false;
  }

  auto symhdr = (Elf64_Sym*)buffer;
  int nelem = size/entsize;
  for (int i=0; i<nelem; i++,symhdr++) {
    if (symhdr->st_name > 0) {
      symtab_lookup[strtab[symhdr->st_name]] = i;
    }
    printf("%d %s %s %s \n", i,
	   stb_name[ELF64_ST_BIND(symhdr->st_info)].data(),
	   stt_name[ELF64_ST_TYPE(symhdr->st_info)].data(),
	   strtab[symhdr->st_name].data());
    // st_name indexes into the symbol table
  }
  return true;
}

void parse_strtab(char* buffer, long size, std::map<int, std::string>& table)
{
  printf("[%s]\n", __func__);
  buffer++;
  size--;
  int npos = 1;
  while (size > 0) {
    table[npos] = buffer;
    int n = table[npos].size() + 1;
    npos += n;
    size -= n;
    buffer += n;
  }
}

bool process_file(void* _vPtr)
{
  printf("%s\n", __func__);

  auto buffer = (char*)_vPtr;
  auto ehdr = (Elf64_Ehdr*)buffer;
  if (!check_ident(ehdr->e_ident)) {
    fprintf(stderr, "invalid header\n");
    return false;
  }

  if (ehdr->e_type != ET_REL) {
    printf("Not relocatable object\n");
    return false;
  }
  if (ehdr->e_machine != EM_X86_64) {
    printf("Not AMD x86-64 arch\n");
    return false;
  }

  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  Elf64_Shdr* sym_shdr = nullptr;
  for (int secno=0; secno<ehdr->e_shnum; secno++) {
    if (shdr->sh_type == SHT_STRTAB) {
      if (secno == ehdr->e_shstrndx)
	parse_strtab(buffer+shdr->sh_offset, shdr->sh_size, shstrtab);
      else
	parse_strtab(buffer+shdr->sh_offset, shdr->sh_size, strtab);
    } else if (shdr->sh_type == SHT_SYMTAB) {
      sym_shdr = shdr;
    }
    shdr++;
  }
  parse_symtab(buffer+sym_shdr->sh_offset, sym_shdr->sh_size,
	       sym_shdr->sh_entsize);
  return true;
}

#if 0
    switch (shdr->sh_type) {
    case SHT_SYMTAB: {
      break;
    }

#endif
int main(int argc, char** argv)
{
  if (argc < 2) {
    printf("usage: %s FILENAME\n", argv[0]);
    exit(-1);
  }

  init();

  int fd = open(argv[1], O_RDWR);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf)) {
    perror("stat");
    exit(1);
  }
  printf("size of %s is %ld bytes\n", argv[1], statbuf.st_size);
  auto elfptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (elfptr == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  
  if (!process_file(elfptr)) {
    printf("failed\n");
    exit(1);
  }
  printf(">>> strtab\n");
  for (auto p = strtab.begin(); p != strtab.end(); p++) {
    printf("%d %s\n", p->first, p->second.data());
  }
  printf(">>> shstrtab\n");
  for (auto p = shstrtab.begin(); p != shstrtab.end(); p++) {
    printf("%d %s\n", p->first, p->second.data());
  }

  exit(0);
}
