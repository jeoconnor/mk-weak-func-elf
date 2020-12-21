#include <iostream>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <elf.h>
#include <map>
#include <vector>
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


static void usage(const char* name)
{
  cout << "help...\n"
       << "more help...\n";
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

void get_last_directory_segment(string& s, const char delim, string& last_seg)
{
  int n = 0;
  int npos = n;
  for (auto p = s.begin(); p != s.end(); p++, n++) {
    if (*p == delim)
      npos = n;
  }
  if (npos > 0)
    npos++;
  last_seg = s.substr(npos);
}

bool file_has_mock_prefix(string& filename, string& prefix)
{
  string dirname;
  get_last_directory_segment(filename, '/', dirname);
  return (prefix == dirname.substr(0, prefix.size()));
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

void create_section_header_string_table(Elf64_Ehdr* ehdr, map<int, string>& table)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  Elf64_Shdr* shstrhdr = shdr + ehdr->e_shstrndx;
  if (shstrhdr->sh_type == SHT_STRTAB) {
    parse_strtab(buffer + shstrhdr->sh_offset, shstrhdr->sh_size, table);
  } else {
    cout << "error: Invalid section header string table: " << shstrhdr->sh_type << endl;
    return;			// exit or throw
  }
}

void create_symbol_table_extra(Elf64_Ehdr* ehdr, map<int, Elf64_Sym*>& symtab,
			 map<string, int>& symtab_lookup, map<int, string>& strtab)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_SYMTAB) {
      if (shdr->sh_entsize != sizeof(Elf64_Sym)) {
	cout << "error: sh_entsize and Elf64_Sym different sizes\n";
	exit(1);
      }
      
      auto symhdr = (Elf64_Sym*)(buffer + shdr->sh_offset);
      int nsyms = shdr->sh_size / shdr->sh_entsize;
      for (int n=0; n < nsyms; n++, symhdr++) {
	if (symhdr->st_name > 0) {
	  symtab_lookup[strtab[symhdr->st_name]] = n;
	  symtab[n] = symhdr;
	}
	// printf("%d %s %s %s \n", i,`
	// 	   stb_name[ELF64_ST_BIND(symhdr->st_info)].data(),
	// 	   stt_name[ELF64_ST_TYPE(symhdr->st_info)].data(),
	// 	   strtab[symhdr->st_name].data());
	// st_name indexes into the symbol table
      }
    }
  }
}

void create_symbol_table(Elf64_Ehdr* ehdr, map<string, Elf64_Sym*>& table,
			 map<int, string>& strtab)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_SYMTAB) {
      if (shdr->sh_entsize != sizeof(Elf64_Sym)) {
	cout << "error: sh_entsize and Elf64_Sym different sizes\n";
	exit(1);
      }
      
      auto symhdr = (Elf64_Sym*)(buffer + shdr->sh_offset);
      int nsyms = shdr->sh_size / shdr->sh_entsize;
      for (int n=0; n < nsyms; n++, symhdr++) {
	if (symhdr->st_name > 0 && ELF64_ST_TYPE(symhdr->st_info) == STT_FUNC) {
	  // Should only add if type equals FUNC
	  table[strtab[symhdr->st_name]] = symhdr;
	}
      }
    }
  }
}

void create_symbol_string_table(Elf64_Ehdr* ehdr, map<int, string>& table)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++) {
    switch (shdr->sh_type) {
    case SHT_STRTAB:
      if (secno != ehdr->e_shstrndx)
	parse_strtab(buffer+shdr->sh_offset, shdr->sh_size, table);
      break;
    default:
      break;
    }
    shdr++;
  }
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

  auto ptr = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED || ptr == nullptr) {
    perror("mmap");
    return {nullptr, 0};
  }

  close(fd);

  return {ptr, statbuf.st_size};
}

tuple<Elf64_Ehdr*, size_t> memory_map_elf_file(string& infile)
{
  auto [ptr, size] =  memory_map_file(infile);
  return {(Elf64_Ehdr*)ptr, size};
}

bool is_mockfile(string& file, string& section_name)
{
  bool found = false;
  auto [ehdr, filesize] = memory_map_elf_file(file);
  if (!ehdr)
    return found;
  char* ptr = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(ptr + ehdr->e_shoff);
  Elf64_Shdr* shstrhdr = shdr + ehdr->e_shstrndx;
  char* buffer = ptr + shstrhdr->sh_offset;
  int bufsize = shstrhdr->sh_size;
  buffer++;
  bufsize--;
  cout << "looking for: " << section_name << endl;
  while (bufsize > 0) {
    cout << "  " << buffer << endl;
    if (section_name == buffer) {
      found = true;
      break;
    }
    // size = strlen(buffer) + 1;
    // bufsize -= size;
    // buffer += size;
  }

  if (munmap(ehdr, filesize) < 0) {
    perror("munmap");
  }

  return found;
}

/**
 * If output file is defined, create it and copy the input file to it
 * and then mmap in the newly created file.  If output file is not
 * defined just mmap in the input file.  Return an Elf64* pointer to
 * the mapped file.
 */
tuple<Elf64_Ehdr*, size_t> memory_map_elf_file_copy(string& infile, string& outfile)
{
  auto [inhdr, size] = memory_map_file(infile);
  if (!inhdr || outfile.size() == 0) {
    return {(Elf64_Ehdr*)inhdr, size};
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
  auto ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED || ptr == nullptr) {
    perror("mmap");
    return {(Elf64_Ehdr*)nullptr, 0};
  }

  // Input file is no longer needed: close and unmap it.
  close(fd);

  if (munmap(inhdr, size) < 0) {
    perror("munmap");
  }

  return {(Elf64_Ehdr*)ptr, size};
}

void show_function_list(Elf64_Ehdr* ehdr, map<string, Elf64_Sym*>& table)
{
  cout << "*** " << __func__ << endl;
  for (auto p = table.begin(); p != table.end(); p++) {
    auto symhdr = p->second;
    if (ELF64_ST_TYPE(symhdr->st_info) == STT_FUNC) {
      auto symhdr = p->second;
      cout << p->first << " " << 
	stb_name[ELF64_ST_BIND(symhdr->st_info)] << " " << 
	stt_name[ELF64_ST_TYPE(symhdr->st_info)] << endl;
    }
  }
  cout << "***\n";
}

void build_symbol_table(Elf64_Ehdr* ehdr, map<string, Elf64_Sym*>& table)
{
  map<int, string> strtab;
  map<int, Elf64_Sym*> symtab;
  map<string, int> symtab_lookup;

  create_symbol_string_table(ehdr, strtab);
  create_symbol_table(ehdr, table, strtab);
}

int main(int argc, char** argv)
{
  string infile;
  string outfile;
  vector<string> mockfiles;
  vector<string> mockfuncs;
  vector<string> objfiles;

  string mock_prefix("mock");
  string section_name(".mock");

  bool list_flag = false;
  bool weak_flag = false;	// This is the point but require explicit request

  int c;
  while ((c = getopt(argc, argv, "i:o:m:f:LW")) != -1) {
    switch (c) {
    case 'i':
      infile = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    case 'm':
      mockfiles.push_back(optarg);
      break;
    case 'f':
      mockfuncs.push_back(optarg);
      break;
    case 'L':
      list_flag = true;
      break;
    case 'W':
      weak_flag = true;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }
#if 0
  while (true) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
        {"input-file",    required_argument, 0, 'i'},
        {"output-file",   required_argument, 0, 'o'},
        {"mock-file",     required_argument, 0, 'm'},
        {"function-name", required_argument, 0, 'f'},
        {"write-flag",    no_argument,       0, 'W'},
	{0,               0,                 0,  0 }
    };
  }
#endif
  if (infile.size() > 0)
    objfiles.push_back(infile);

  while (optind < argc) {
    string s = argv[optind];
    if (file_has_mock_prefix(s, mock_prefix))
      mockfiles.push_back(s);
    else
      objfiles.push_back(s);
    optind++;
  }

  vector<string> nonmockfiles;
  for(auto p = objfiles.begin(); p != objfiles.end(); p++) {
    cout << "p=" << *p << endl;
    if (is_mockfile(*p, section_name)) {
      mockfiles.push_back(*p);
    } else {
      nonmockfiles.push_back(*p);
    }
  }
  objfiles.erase(objfiles.begin(), objfiles.end());

  init_tables();

  /*
   * First build up a list of function names we want to superceed from
   * the list of mock files.
   */
  map<string, Elf64_Sym*> mock_symtab;
  for (auto pFile = mockfiles.begin(); pFile != mockfiles.end(); pFile++) {
    auto [ehdr, size] = memory_map_elf_file(*pFile);
    if (!verify_elf(ehdr)) {
      cout << "error: invalid elf file " << *pFile << endl;
      exit(1);
    }

    build_symbol_table(ehdr, mock_symtab);
    for (auto p = mock_symtab.begin(); p != mock_symtab.end(); p++) {
      mockfuncs.push_back(p->first);
    }
    // Done with the file, unmap it
    if (munmap(ehdr, size) != 0) {
      perror("munmap");
    }
  }
  // I don't know of this matters
  mock_symtab.erase(mock_symtab.begin(), mock_symtab.end());

  // See what we got
  for(auto p = mockfuncs.begin(); p != mockfuncs.end(); p++) {
    cout << *p << endl;
  }
  
  for(auto pFile = nonmockfiles.begin(); pFile != nonmockfiles.end(); pFile++) {
    map<string, Elf64_Sym*> symbol_table;

    cout << "> " << *pFile << endl;
    auto [ehdr, size] = memory_map_elf_file_copy(*pFile, outfile);
    if (!verify_elf(ehdr)) {
      cout << "error: invalid elf file " << *pFile << endl;
      exit(1);
    }
    if (outfile.size() > 0) {
      /* This is a little hoky, but since only one outfile can be
	 defined at the moment, null it out after first use.
       */
      outfile = "";
    }

    // create_section_header_string_table(ehdr, shstrtab);
    build_symbol_table(ehdr, symbol_table);
    // add_any_mock_symbols(mock_ehdr);

    if (list_flag) {
      // if (mock_ehdr != nullptr)
      //   show_symbol_table(mock_ehdr);
      // else
      show_function_list(ehdr, symbol_table);
    }

    if (weak_flag) {
      for (auto p = mockfuncs.begin(); p < mockfuncs.end(); p++) {
	cout << "patching " << *p << endl;
	auto shdr = symbol_table[*p];
	if (shdr) {
	  // printf("st_info 0x%08x\n", shdr->st_info);
	  shdr->st_info = ELF64_ST_INFO(STB_WEAK, ELF64_ST_TYPE(shdr->st_info));
	  // printf("st_info 0x%08x\n", shdr->st_info);
	}
      }

      if (msync(ehdr, size, MS_SYNC) != 0) {
	perror("msync");
      }
    }

    if (munmap(ehdr, size) != 0) {
      perror("munmap");
    }
  }

  exit(0);
}
