/* These are declarations of the functions which implement Dino
   environment functions. */

extern void min_call (int_t pars_number);
extern void max_call (int_t pars_number);
extern void tolower_call (int_t pars_number);
extern void toupper_call (int_t pars_number);
extern void eltype_call (int_t pars_number);
extern void keys_call (int_t pars_number);
extern void context_call (int_t pars_number);
extern void inside_call (int_t pars_number);
extern void match_call (int_t pars_number);
extern void rcount_call (int_t pars_number);
extern void sub_call (int_t pars_number);
extern void gsub_call (int_t pars_number);
extern void split_call (int_t pars_number);
extern void subv_call (int_t pars_number);
extern void cmpv_call (int_t pars_number);
extern void del_call (int_t pars_number);
extern void ins_call (int_t pars_number);
extern void insv_call (int_t pars_number);
extern void sort_call (int_t pars_number);
extern void rename_call (int_t pars_number);
extern void remove_call (int_t pars_number);
extern void mkdir_call (int_t pars_number);
extern void rmdir_call (int_t pars_number);
extern void getcwd_call (int_t pars_number);
extern void chdir_call (int_t pars_number);
extern void chumod_call (int_t pars_number);
extern void chgmod_call (int_t pars_number);
extern void chomod_call (int_t pars_number);
extern void isatty_call (int_t pars_number);
extern void open_call (int_t pars_number);
extern void close_call (int_t pars_number);
extern void flush_call (int_t pars_number);
extern void popen_call (int_t pars_number);
extern void pclose_call (int_t pars_number);
extern void tell_call (int_t pars_number);
extern void seek_call (int_t pars_number);
extern void put_call (int_t pars_number);
extern void putln_call (int_t pars_number);
extern void fput_call (int_t pars_number);
extern void fputln_call (int_t pars_number);
extern void print_call (int_t pars_number);
extern void println_call (int_t pars_number);
extern void fprint_call (int_t pars_number);
extern void fprintln_call (int_t pars_number);
extern void get_call (int_t pars_number);
extern void getln_call (int_t pars_number);
extern void getf_call (int_t pars_number);
extern void fget_call (int_t pars_number);
extern void fgetln_call (int_t pars_number);
extern void fgetf_call (int_t pars_number);
extern void scan_call (int_t pars_number);
extern void scanln_call (int_t pars_number);
extern void fscan_call (int_t pars_number);
extern void fscanln_call (int_t pars_number);
extern void getpid_call (int_t pars_number);
extern void getun_call (int_t pars_number);
extern void geteun_call (int_t pars_number);
extern void getgn_call (int_t pars_number);
extern void getegn_call (int_t pars_number);
extern void getgroups_call (int_t pars_number);
extern void sqrt_call (int_t pars_number);
extern void exp_call (int_t pars_number);
extern void log_call (int_t pars_number);
extern void log10_call (int_t pars_number);
extern void pow_call (int_t pars_number);
extern void sin_call (int_t pars_number);
extern void cos_call (int_t pars_number);
extern void atan2_call (int_t pars_number);
extern void rand_call (int_t pars_number);
extern void srand_call (int_t pars_number);
extern void readdir_call (int_t pars_number);
extern void ftype_call (int_t pars_number);
extern void fun_call (int_t pars_number);
extern void fgn_call (int_t pars_number);
extern void fsize_call (int_t pars_number);
extern void fatime_call (int_t pars_number);
extern void fmtime_call (int_t pars_number);
extern void fctime_call (int_t pars_number);
extern void fumode_call (int_t pars_number);
extern void fgmode_call (int_t pars_number);
extern void fomode_call (int_t pars_number);
extern void time_call (int_t pars_number);
extern void strtime_call (int_t pars_number);
extern void clock_call (int_t pars_number);
extern void gc_call (int_t pars_number);
extern void system_call (int_t pars_number);
extern void exit_call (int_t pars_number);
extern void init_call (int_t pars_number);

extern void int_earley_parse_grammar (int_t pars_number);
extern void int_earley_set_debug_level (int_t pars_number);
extern void int_earley_set_one_parse_flag (int_t pars_number);
extern void int_earley_set_error_recovery_flag (int_t pars_number);
extern void int_earley_set_recovery_match (int_t pars_number);
extern void int_earley_parse (int_t pars_number);
extern void int_earley_create_grammar (int_t pars_number);
