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

// These are used for displaying field names
map<int, string> stt_name;
map<int, string> stb_name;
map<int, string> sht_name;

tuple<void*, size_t> memory_map_file(string& file);
unsigned char verify_elf(void* hdr);

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void process_files(vector<string>& infiles, vector<string>& dupfiles, vector<string>& funclist,
		   string& section_name, bool write_flag);

/*
 * Sets the BIND attribute in the symbol table to WEAK for functions
 * in the function_names list.
 */
template <typename ElfNN_Sym>
void patch_file(ElfNN_Sym *shdr, char* symbuf, vector<string>& function_names);


template <typename ElfNN_Ehdr>
class ElfFile {
public:
  ElfFile(string& _filename) {
    init(_filename);
  }

  ~ElfFile() {
    deinit();
  }

  void init(string& _filename) {
    filename = _filename;
    auto [_ehdr, _size] = memory_map_file(filename);
    ehdr = (ElfNN_Ehdr*)_ehdr;
    size = _size;
    if (!verify_elf(ehdr)) {
      cout << "error: invalid elf file " << filename << endl;
      deinit();
    }
  }

  char check_arch() {
    return ehdr->e_ident[EI_CLASS];
  }

  void deinit() {
    if (ehdr) {
      if (munmap(ehdr, size) < 0) {
	perror("munmap");
      }
      ehdr = nullptr;
      size = 0;
    }
  }

  bool ok() { return ehdr != nullptr && size > 0; }

  ElfNN_Ehdr* Handle() { return ehdr; }
  int Size() { return size; }

  string filename;
  int size;
  ElfNN_Ehdr* ehdr;

};

static string basename(string& argv0)
{
  int npos = 0;
  int nstart = 0;
  for (auto p = argv0.begin(); p != argv0.end(); p++, npos++) {
    if (*p == '/')
      nstart = npos + 1;
  }
  return argv0.substr(nstart, string::npos);
}

void usage(string argv0)
{
  string progname = basename(argv0);

  cout << "Usage: " << progname << " [Options] OBJFILES\n" <<
    "\nDESCRIPTION\n" <<
    "Modifies a set of Elf format relocatable object files to allow linking with additional\n" <<
    "object files containing duplicated functions.  The purpose is to enable building test cases\n" <<
    "using test doubles (mocks, stubs, etc.) without having to modify the original sources.\n" <<
    "Supports both Elf32 and Elf64 formats and has been tested on X86_64 and ARM processors.\n\n" <<
    "OBJFILES                              List of either Elf32 or Elf64 relocatable object files to\n" <<
    "                                      be optionally modified.\n" <<
    "\nOPTIONS:\n" <<
    " -s --section-name=SECTION_NAME       Defines an alternate Elf text section (default .mock)\n" <<
    "                                      in which test double functions will have been placed.\n" <<
    " -r --replacement-file=TEST_DOUBLE_FILE   Elf relocatable object file containing function test\n" <<
    "                                      doubles. They need not be labeled with a section attribute.\n" <<
    "                                      Option may be invoked multiple times.\n" <<
    " -p --prefix-name=PREFIX              Any filenames prefixed with PREFIX will be treated as\n" <<
    "                                      test double files (default mock).\n" <<
    " -w --write-flag                      Will set WEAK binding for selected functions\n" <<
    "                                      in OBJFILES: excluding those with a text section\n" <<
    "                                      labeled SECTION_NAME.\n" <<
    " -l --list                            List function test doubles.\n" <<
    " -h --help                            This help.\n\n";
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

/*
 * Returns a pointer to the start of the section header table.
 */
template <typename ElfNN_Shdr, typename ElfNN_Ehdr>
ElfNN_Shdr* get_section_header(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  return (ElfNN_Shdr*)(buffer + ehdr->e_shoff);
}

/*
 * Finds the start of the symbol table and returns a tuple of the
 * pointer to the first byte of the table and the number of entries.
 */
template <typename ElfNN_Shdr, typename ElfNN_Sym, typename ElfNN_Ehdr>
tuple<ElfNN_Sym*, int> get_symbol_table(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  auto shdr = get_section_header<ElfNN_Shdr>(ehdr);
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_SYMTAB) {
      int nsyms = shdr->sh_size / shdr->sh_entsize;
      return {(ElfNN_Sym*)(buffer + shdr->sh_offset), nsyms};
    }
  }
  return {nullptr, 0};
}

/*
 * Returns a pair of char pointers to the beginning of the symbol and
 * section header string buffers, respectively. For LLVM at least, the
 * two pointers will be the same.
 */
template<typename ElfNN_Shdr, typename ElfNN_Ehdr>
tuple<char*, char*> get_string_buffers(ElfNN_Ehdr* ehdr)
{
  char* ebuf = (char*)ehdr;
  char* symbuf = nullptr;
  char* shbuf = nullptr;
  ElfNN_Shdr* shdr = (ElfNN_Shdr*)(ebuf + ehdr->e_shoff);
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_STRTAB) {
      if (secno == ehdr->e_shstrndx) {
	shbuf = (char*)(ebuf + shdr->sh_offset);
      } else {
	symbuf = (char*)(ebuf + shdr->sh_offset);
      }
    }
  }
  // LLVM only defines a single string buffer
  if (symbuf == nullptr)
    symbuf = shbuf;

  return {symbuf, shbuf};
}

/*
 * Converts strings like a/b/c => c, ./a => a, and b => b.
 *
 * 
 */
string get_last_directory_segment(string& s, const char delim)
{
  int n = 0;
  int npos = n;
  for (auto p = s.begin(); p != s.end(); p++, n++) {
    if (*p == delim)
      npos = n;
  }
  if (npos > 0)
    npos++;
  return s.substr(npos);
}

bool file_has_select_prefix(string& filename, string& prefix)
{
  if (prefix.size() > 0) {
    auto dirname = get_last_directory_segment(filename, '/');
    return (prefix == dirname.substr(0, prefix.size()));
  }
  return false;
}

/**
 * Check if input ptr references a valid elf file.  Returns either
 * ELFCLASS32 or ELFCLASS64 on sucess or ELFCLASSNONE on failure.
 */
unsigned char verify_elf(void* ptr)
{
  unsigned char ei_class = ELFCLASSNONE;

  if (ptr == nullptr) {
    printf("error: no file found\n");
    return ei_class;
  }

  auto hdr = (Elf64_Ehdr*)ptr;

  if (hdr->e_ident[EI_MAG0] != ELFMAG0 &&
      hdr->e_ident[EI_MAG1] != ELFMAG1 &&
      hdr->e_ident[EI_MAG2] != ELFMAG2 &&
      hdr->e_ident[EI_MAG3] != ELFMAG3) {
    cout << "error: Elf magic number not found\n";
    return ei_class;
  }

  if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
    cout << "error: file must be 2's complement, little-endian\n";
    return ei_class;
  }

  // Relocatable object?
  if (hdr->e_type != ET_REL) {
    cout << "error: file must be relocatable object\n";
    return ei_class;
  }

  /* check the class last since its value is returned on success */
  ei_class = hdr->e_ident[EI_CLASS];
  if ((ei_class != ELFCLASS32) &&
      (ei_class != ELFCLASS64)) {
    cout << "error: only Elf32 and Elf64 architectures supported\n";
    return ei_class;
  }

  return ei_class;
}

/*
 * Determines if the file is ELFCLASS32, ELFCLASS64 or ELFCLASSNONE
 * (none of the above) and returns one of these values.
 *
 * At this point the choice of Elf header structure type doesn't
 * matter, that is, either the 32 or 64 bit version will work since
 * only the commen part of the header is being examined.
 */
char check_arch(string& filename)
{
  ElfFile<Elf64_Ehdr> elfFile(filename);
  return elfFile.check_arch();
}

/*
 * Memory map in the file.  Returns a tuple of a pointer to the start
 * of the file and the size of the file.
 */
tuple<void*, size_t> memory_map_file(string& file)
{
  if (file.size() == 0)
    return {nullptr, 0};

  int fd = open(file.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open");
    return {nullptr, 0};
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf)) {
    perror("stat");
    return {nullptr, 0};
  }

  auto ptr = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED || ptr == nullptr) {
    perror("mmap");
    return {nullptr, 0};
  }

  /*
   * No longer need to leave the file open once the it is mapped in.
   */
  close(fd);

  return {ptr, statbuf.st_size};
}

/*
 * Convenience wrapper around
 * 'tuple<void*, size_t> memory_map_file(string& file)'
 */
tuple<Elf64_Ehdr*, size_t> memory_map_elf_file(string& infile)
{
  auto [ptr, size] =  memory_map_file(infile);
  return {(Elf64_Ehdr*)ptr, size};
}

inline bool check_symbol_type(Elf64_Sym* sym) {
  return (sym->st_name > 0) &&
    (ELF64_ST_TYPE(sym->st_info) == STT_FUNC) &&
    (ELF64_ST_BIND(sym->st_info) == STB_GLOBAL);
}
  
inline bool check_symbol_type(Elf32_Sym* sym) {
  return (sym->st_name > 0) &&
    (ELF32_ST_TYPE(sym->st_info) == STT_FUNC) &&
    (ELF32_ST_BIND(sym->st_info) == STB_GLOBAL);
}

/*
 * Returns the index in the Elf Section header table whose names
 * matches section_name.
 */
template<typename ElfNN_Shdr, typename ElfNN_Ehdr>
int get_section_index(ElfNN_Ehdr *ehdr, string& section_name)
{
  int section_index = 0;
  /*
   * Scan through the section header string table looking for the
   * section with the name matching section_name
   */
  auto [_, shstrbuf] = get_string_buffers<ElfNN_Shdr>(ehdr);
  if (section_name.size() > 0) {
    // Find section index for custom section
    auto shdr = get_section_header<ElfNN_Shdr>(ehdr);
    for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
      if (shdr->sh_name != 0) {
	if (section_name == shstrbuf + shdr->sh_name) {
	  section_index = secno;
	  // cout << "custom index is " << section_index << endl;
	  break;
	}
      }
    }
  }
  return section_index;
}
  
/*
 * Find the global functions defined in the symbolt table that index
 * to the Elf section named section_name.
 * Found function names are added to function_names.
 */
template<typename ElfNN_Shdr, typename ElfNN_Sym, typename ElfNN_Ehdr>
bool extract_function_names(ElfNN_Ehdr* ehdr, string& section_name,
			    vector<string>& function_names)
{
  bool found = false;

  int initial_function_number = function_names.size();

  // Section header
  if (!ehdr->e_shoff) {
    cout << "error: unable to find section header table\n";
    return false;
  }

  auto [strbuf, _] = get_string_buffers<ElfNN_Shdr>(ehdr);
  int section_index = get_section_index<ElfNN_Shdr>(ehdr, section_name);

  /*
   * Look for symbols referencing the special section
   */
  auto [symhdr, numsyms] = get_symbol_table<ElfNN_Shdr, ElfNN_Sym>(ehdr);
  for (int n=0; n < numsyms; n++, symhdr++) {
    if (check_symbol_type(symhdr)) {
      if (symhdr->st_shndx == section_index || section_name.size() == 0) {
	string name = strbuf + symhdr->st_name;
	cout << "test double list <= " << name << endl;
	function_names.push_back(name);
      }
    }
  }

  if (function_names.size() > initial_function_number)
    found = true;

  return found;
}

template <>
void patch_file(Elf64_Sym *shdr, char* symbuf, vector<string>& function_names)
{
  // Maybe check also that it is GLOBAL?
  // if (shdr->st_name != 0 && ELF64_ST_TYPE(shdr->st_info) == STT_FUNC) {
  if (check_symbol_type(shdr)) {
    for (auto p=function_names.begin(); p!=function_names.end(); p++) {
      string func_name(symbuf + shdr->st_name);
      if (*p == func_name) {
	// cout << "patching " << func_name << " in " << *pFile << endl;
	shdr->st_info = ELF64_ST_INFO(STB_WEAK, ELF64_ST_TYPE(shdr->st_info));
      }
    }
  }
}

template<>
void patch_file(Elf32_Sym *shdr, char* symbuf, vector<string>& function_names)
{
  // Maybe check also that it is GLOBAL?
  // if (shdr->st_name != 0 && ELF32_ST_TYPE(shdr->st_info) == STT_FUNC) {
  if (check_symbol_type(shdr)) {
    for (auto p=function_names.begin(); p!=function_names.end(); p++) {
      string func_name(symbuf + shdr->st_name);
      if (*p == func_name) {
	// cout << "patching " << func_name << " in " << *pFile << endl;
	shdr->st_info = ELF32_ST_INFO(STB_WEAK, ELF32_ST_TYPE(shdr->st_info));
      }
    }
  }
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void patch_files(vector<string>& objfiles, vector<string>& function_names)
{
  for(auto pFile = objfiles.begin(); pFile != objfiles.end(); pFile++) {
    ElfFile<ElfNN_Ehdr> elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    auto [symbuf, _] = get_string_buffers<ElfNN_Shdr>(ehdr);
    auto [symhdr, nsyms] = get_symbol_table<ElfNN_Shdr, ElfNN_Sym>(ehdr);
    for (int idx=0; idx < nsyms; idx++, symhdr++) {
      patch_file<ElfNN_Sym>(symhdr, symbuf, function_names);
    }

    if (msync(ehdr, elfFile.Size(), MS_SYNC) != 0) {
      perror("msync");
    }
  }
}

template<typename ElfNN_Shdr, typename ElfNN_Sym, typename ElfNN_Ehdr>
void extract_function_names(ElfNN_Ehdr* ehdr, vector<string>& funclist)
{
  string secname("");
  (void)extract_function_names<ElfNN_Shdr, ElfNN_Sym>(ehdr, secname, funclist);
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void extract_function_names(vector<string>& dupfiles, vector<string>& funclist)
{
  for (auto pFile = dupfiles.begin(); pFile != dupfiles.end(); pFile++) {
    ElfFile<ElfNN_Ehdr> elfFile(*pFile);
    
    auto ehdr = elfFile.Handle();
    if (ehdr)
      extract_function_names<ElfNN_Shdr, ElfNN_Sym>(ehdr, funclist);
  }
}

/**
 * Extract from infiles function names contained in funclist that are
 * located in the section_name text section and save those files to
 * outfiles.  
 * @param infiles list of input filenames 
 * @param funclist list of function names 
 * @param section_name the name of a labeled elf text section.
 * Labeled in C/C++ code with the attribute,
 * `__attribute__((section("NAME")))`
 */
template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void extract_labeled_function_names(vector<string>& infiles, vector<string>& funclist,
				    string& section_name, vector<string>& outfiles)
{
  for(auto pFile = infiles.begin(); pFile != infiles.end(); pFile++) {
    ElfFile<ElfNN_Ehdr> elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    if (!extract_function_names<ElfNN_Shdr, ElfNN_Sym>(ehdr, section_name, funclist)) {
      outfiles.push_back(*pFile);
    }
  }
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void process_files(vector<string>& infiles, vector<string>& dupfiles, vector<string>& funclist,
		   string& section_name, bool write_flag)
{
  /**
   * First build up a list of function names we want to replace from
   * the list of explicit mock files.  All global function names
   * defined in these files will be included.
   */
  extract_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(dupfiles, funclist);

  /**
   * Identify object files with an identifed, i.e. labeled, section
   * and add those function names to the function list.
   * Also, saves the list of objfiles that did not contain labeled
   * sections.
   */
  vector<string> objfiles;	// candidate files for modification
  extract_labeled_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(infiles, funclist, section_name, objfiles);

  /**
   * The non-mock files are the ones to modify
   */
  if (write_flag)
    patch_files<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(objfiles, funclist);

}

int main(int argc, char** argv)
{
  vector<string> dupfiles;	// contain replacement function definitions
  vector<string> infiles;	// unclassified input files
  vector<string> funclist;	// use unordered_set?

  string prefix_name("mock");
  string section_name(".mock");

  bool list_flag = false;
  bool write_flag = false;	// This is the point but require explicit request

  int c;
  while (true) {
    // int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
      {"replacement-file", required_argument, 0, 'r'},
      {"function-name",    required_argument, 0, 'f'},
      {"section-name",     required_argument, 0, 's'},
      {"prefix-name",      required_argument, 0, 'p'},
      {"write-flag",       no_argument,       0, 'w'},
      {"list",             no_argument      , 0, 'l'},
      {"help",             no_argument      , 0, 'h'},
      {0,               0,                 0,  0 }
    };

    c = getopt_long(argc, argv, "r:f:s:wlh", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'r':
      dupfiles.push_back(optarg);
      break;
    case 'f':
      funclist.push_back(optarg);
      break;
    case 'l':
      list_flag = true;
      break;
    case 'w':
      write_flag = true;
      break;
    case 's':
      section_name = optarg;
      break;
    case 'p':
      prefix_name = optarg;
      break;
    case 'h':
      usage(argv[0]);
      return 0;
      break;
    default:
      usage(argv[0]);
      return -1;
      break;
    }
  }

  while (optind < argc) {
    string s = argv[optind];
    if (file_has_select_prefix(s, prefix_name))
      dupfiles.push_back(s);
    else
      infiles.push_back(s);
    optind++;
  }

  init_tables();

  /*
   * Figure out if we have elf32 or elf64 files by checking the first
   * one.
   */
  switch (check_arch(*(infiles.begin()))) {
  case ELFCLASS32:
    process_files<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym>(infiles, dupfiles, funclist, section_name, write_flag);
    break;
  case ELFCLASS64:
    process_files<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym>(infiles, dupfiles, funclist, section_name, write_flag);
    break;
  case ELFCLASSNONE:
  default:
    exit(1);
    break;
  }

  if (list_flag) {
    for_each(funclist.begin(), funclist.end(), [](auto p){ cout << p << endl; });
    exit(0);
  }

  exit(0);
}
