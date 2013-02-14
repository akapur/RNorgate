#include <Rcpp.h>

RcppExport SEXP createNorgateReader();
RcppExport SEXP open_directory(SEXP rcpp_norgate_ptr, SEXP rcpp_dir_name);
RcppExport SEXP open_emaster(SEXP rcpp_norgate_ptr, SEXP rcpp_emaster_name);
RcppExport SEXP get_emasters(SEXP rcpp_norgate_ptr);
RcppExport SEXP available_assets(SEXP rcpp_norgate_ptr);
RcppExport SEXP get_data_from_file(SEXP rcpp_norgate_ptr, SEXP rcpp_data_filename);

