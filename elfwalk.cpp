#include <iostream>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <elf.h>
#include <map>
#include <string>
#include <tuple>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>

using namespace std;

using TableIntStr = map<int, string>;
using TableStrInt = map<string, int>;

// using SymbolTable = Table

// These are used for displaying field names
map<int, string> stt_name;
map<int, string> stb_name;
map<int, string> sht_name;

// symbol string table
map<int, string> strtab;

// section header string table
map<int, string> shstrtab;

//map<int, string> symtab;
// returns an index into the symbol table for the given symbol
map<string, int> symtab_lookup;

static void usage(const char* name)
{
}

/**
 * For now just define enough entries to get by with my easy test
 * cases but fill these out later.
 */
static void init_tables()
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

static bool verify_elf(Elf64_Ehdr* hdr)
{
  if (hdr == nullptr) {
    printf("error: no file found\n");
    return false;
  }
  // Elf header?
  bool ok = (hdr->e_ident[EI_MAG0] == ELFMAG0 &&
	     hdr->e_ident[EI_MAG1] == ELFMAG1 &&
	     hdr->e_ident[EI_MAG2] == ELFMAG2 &&
	     hdr->e_ident[EI_MAG3] == ELFMAG3);

  // Relocatable object?
  ok = ok && (hdr->e_type == ET_REL);

  if (!ok) {
    printf("error: file not Elf relocatable object\n");
    return false;
  }

  return true;
}

/**
 * Receives a pointer to the start of a Elf64_Sym buffer and the name
 * of a sumbole to find.  If the symbol is found then the return value
 * is a pointer to the Elf64 record in the buffer or NULL;
 */
Elf64_Sym* find_symbol_entry(Elf64_Sym* buffer, string& name)
{
  int idx = symtab_lookup[name];
  if (idx == 0)
    return nullptr;

  return buffer + idx;
}

bool parse_symtab_lookup(char* buffer, long size, long entsize)
{
  cout << "** " << __func__ << endl;
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

void parse_strtab(char* buffer, long size, map<int, string>& table)
{
  cout << "** " << __func__ << endl;
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

void create_section_header_string_table(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  Elf64_Shdr* shstrhdr = shdr + ehdr->e_shstrndx;
  if (shstrhdr->sh_type == SHT_STRTAB) {
    parse_strtab(buffer + shstrhdr->sh_offset, shstrhdr->sh_size, shstrtab);
  } else {
    cout << "error: Invalid section header string table: " << shstrhdr->sh_type << endl;
    return;			// exit or throw
  }
}

void create_symbol_table(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++) {
    switch (shdr->sh_type) {
    case SHT_SYMTAB:
      parse_symtab_lookup(buffer+shdr->sh_offset, shdr->sh_size, shdr->sh_entsize);
      break;
    default:
      break;
    }
    shdr++;
  }
}

void create_symbol_string_table(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++) {
    switch (shdr->sh_type) {
    case SHT_STRTAB:
      if (secno != ehdr->e_shstrndx)
	parse_strtab(buffer+shdr->sh_offset, shdr->sh_size, strtab);
      break;
    default:
      break;
    }
    shdr++;
  }
}

bool process_file(Elf64_Ehdr* ehdr)
{
  if (ehdr->e_machine != EM_X86_64) {
    printf("Not AMD x86-64 arch\n");
    return false;
  }

  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  Elf64_Shdr* sym_shdr = nullptr;
  for (int secno=0; secno<ehdr->e_shnum; secno++) {
    switch (shdr->sh_type) {
    case SHT_STRTAB:
      {
	static int strtab_count = 0;
	if (secno == ehdr->e_shstrndx) {
	  parse_strtab(buffer+shdr->sh_offset, shdr->sh_size, shstrtab);
	} else {
	  if (strtab_count++ > 0) {
	    printf("error: more than one string table (not including the\n"
		   "section header string table) currently supported.\n");
	    return false;
	  }
	  parse_strtab(buffer+shdr->sh_offset, shdr->sh_size, strtab);
	}
	break;
      }
    case SHT_SYMTAB:
      sym_shdr = shdr;
      break;
    default:
      break;
    }
    shdr++;
  }
  parse_symtab_lookup(buffer+sym_shdr->sh_offset, sym_shdr->sh_size,
	       sym_shdr->sh_entsize);
  return true;
}

static tuple<void*, size_t> memory_map_file(string& file)
{
  if (file.size() == 0)
    return {nullptr, 0};

  int fd = open(file.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf)) {
    perror("stat");
    exit(1);
  }

  auto ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (ptr == MAP_FAILED || ptr == nullptr) {
    perror("mmap");
    return {nullptr, 0};
  }

  close(fd);

  return {ptr, statbuf.st_size};
}

Elf64_Ehdr* memory_map_elf_file(string& infile)
{
  auto [ptr, _] = memory_map_file(infile);
  return (Elf64_Ehdr*)ptr;
}
/**
 * If output file is defined, create it and copy the input file to it
 * and then mmap in the newly created file.  If output file is not
 * defined just mmap in the input file.  Return an Elf64* pointer to
 * the mapped file.
 */
Elf64_Ehdr* memory_map_elf_file_copy(string& infile, string& outfile)
{
  auto [inhdr, size] = memory_map_file(infile);
  if (!inhdr || outfile.size() == 0) {
    return (Elf64_Ehdr*)inhdr;
  }

  int fd = open(outfile.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0644);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // copy input file to output file
  // just in case:
  auto bytes_to_write = size;
  auto inbuf = (char*) inhdr;
  while (bytes_to_write > 0) {
    auto bytes_written = write(fd, inhdr, bytes_to_write);
    if (bytes_written < 0) {
      perror("write");
      exit(1);
    }
    bytes_to_write -= bytes_written;
    inbuf += bytes_written;
  }

  // map in new output file
  auto ptr = (Elf64_Ehdr*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (ptr == MAP_FAILED || ptr == nullptr) {
    perror("mmap");
    return (Elf64_Ehdr*)nullptr;
  }

  // Input file is no longer needed: close and unmap it.
  close(fd);

  if (munmap(inhdr, size) < 0) {
    perror("munmap");
  }

  return ptr;
}

void show_symbol_table(Elf64_Ehdr* ehdr)
{
  (void)ehdr;
}

int main(int argc, char** argv)
{
  string infile;
  string outfile;
  string mockfile;
  bool list_flag = false;
  int c;
  while ((c = getopt(argc, argv, "i:o:m:L")) != -1) {
    switch (c) {
    case 'i':
      infile = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    case 'm':
      mockfile = optarg;
      break;
    case 'L':
      list_flag = true;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  init_tables();

  Elf64_Ehdr* ehdr = memory_map_elf_file_copy(infile, outfile);
  if (!verify_elf(ehdr)) {
    cout << "error: invalid elf file " << infile << endl;
    exit(1);
  }

  Elf64_Ehdr* mock_ehdr = memory_map_elf_file(mockfile);
  if (mock_ehdr != nullptr && !verify_elf(mock_ehdr)) {
    cout << "error: invalid elf file " << infile << endl;
    exit(1);
  }


  create_section_header_string_table(ehdr);
  create_symbol_string_table(ehdr);
  create_symbol_table(ehdr);

  if (list_flag) {
    if (mock_ehdr != nullptr)
      show_symbol_table(mock_ehdr);
    else
      show_symbol_table(ehdr);
    return 0;
  }

  // if (!process_file(ehdr)) {
  //   printf("failed\n");
  //   exit(1);
  // }
  printf(">>> symbol string table\n");
  for (auto p = strtab.begin(); p != strtab.end(); p++) {
    cout << p->first << " " << p->second << endl;
  }
  printf(">>> section header symbol table\n");
  for (auto p = shstrtab.begin(); p != shstrtab.end(); p++) {
    cout << p->first << " " << p->second << endl;
  }
  cout << ">>> symbol table lookup\n";
  for_each(symtab_lookup.begin(), symtab_lookup.end(),
	  [&](auto& p) {
	    cout << p.first << " " << p.second << endl;
	  });

  exit(0);
}
