
extern char *iparser; // Current Parser Index

void parse_spaces();
int parse_value(char *str, char *delimiters);
int parse_str(char *str);
int parse_name(char *str);
int parse_int(char *str);
int parse_hex(char *str);
int parse_bin(char *str);
int parse_expect( char c );
