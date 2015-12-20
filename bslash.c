
#define AS_OPTION_USAGE_MESSAGE "\
Usage: %s [options] file...\n\
  Assemble an ELF object.\n\n\
  Options:\n\
  -D                        Print assembler debug messages.\n\
  -I DIR                    Add directory to the search list.\n\
  -J                        Suppress warnings about signed overflows.\n\
  -K                        Warn about alterations to difference tables.\n\
  -L | --keep-locals        Keep local symbols.\n\
  -R                        Merge the data and text sections.\n\
  -V                        Display the assembler version number.\n\
  -W | --no-warn            Suppress warnings.\n\
  -Z                        Generate the object even if there are errors.\n\
  -a[listing-options...]    Control assembler listings.\n\
  -g | --gen-debug          Generate debugging information.\n\
  -h | --help               Show a help message.\n"

main()
{
	printf(AS_OPTION_USAGE_MESSAGE);
}
