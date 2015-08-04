#include <Rcpp.h>

#include <string>
#include <map>
#include <set>
#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/param.h>



class NorgateDirectory
{
public:
    NorgateDirectory()
    {
        reset_refdata();
    };

    ~NorgateDirectory()
    {
    };


    int add_directory(const char *dir_name)
    {
        char error[1024];

        std::set< std::string > emasters_found;
        int fs_stat = find_emaster_files(dir_name, emasters_found);
        if(0 != fs_stat)
        {
            sprintf(error,
                   "Could not search for emaster_files in dir %s. Errno: %d",
                   dir_name, errno);
            throw(Rcpp::exception(error));
            return fs_stat;
        }
        std::set< std::string >::iterator itr     = emasters_found.begin();
        std::set< std::string >::iterator end_itr = emasters_found.end();

        for(; itr != end_itr; ++itr)
        {
            int add_stat = add_emaster_file((itr->c_str()));
            if(0 != add_stat)
            {
                sprintf(error, "Could not read emaster file %s, Errno: %d",
                        itr->c_str(), errno);
                throw(Rcpp::exception(error));
                return add_stat;
            }
        }

        return 0;
    };


    int find_emaster_files(const char *dir_name, std::set< std::string > &emasters)
    {
        char error[1024];

        char abs_dir_name[MAXPATHLEN];
        if(get_absolute_path(dir_name, abs_dir_name))
        {
            sprintf(error, "Error in get_absolute_path");
            throw(Rcpp::exception(error));
            return errno;
        }

        DIR  *dp = opendir(abs_dir_name);
        if(NULL == dp)
        {
            sprintf(error, "Cannot open directory: %s, Errno: %d.", dir_name, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        dirent *dirp;
        while((dirp = readdir(dp)) != NULL)
        {
            char full_path[MAXPATHLEN];
            strcpy(full_path, abs_dir_name);
            strcat(full_path, "/"); 
            strcat(full_path, dirp->d_name);

            if(DT_DIR == dirp->d_type)
            {
                if((0 != strcmp(dirp->d_name, ".")) &&
                   (0 != strcmp(dirp->d_name, "..")) )
                {
                    int ret_subdir = find_emaster_files(full_path, emasters);
                    if(0 != ret_subdir)
                    {
                        sprintf(error, "Could not process subdir: %s, Errno: %d",
                            full_path, errno);
                        throw(Rcpp::exception(error));
                        return errno;
                    }
                }
            }

            if(DT_REG == dirp->d_type)
            {
                if(0 == strcasecmp(dirp->d_name, "emaster"))
                {
                    if(emasters.find(full_path) == emasters.end())
                    {
                        emasters.insert(full_path);
                    }
                }
            }
        }
        if(closedir(dp))
        {
            sprintf(error, "Failure on closedir, Errno: %d.", errno);
            throw(Rcpp::exception(error));
            return errno;
        }
        return 0;
    };


    int add_emaster_file(const char *file_name)
    {
        char error[1024];

        size_t file_name_len = strlen(file_name);
        char dir_name[MAXPATHLEN];
        strncpy(dir_name, file_name, (file_name_len-7));
        dir_name[file_name_len-7] = '\0';

        char abs_dir_name[MAXPATHLEN];
        if(0 != get_absolute_path(dir_name, abs_dir_name))
        {
            sprintf(error, "Error in get_absolute_path for dir %s, Errno: %d.", dir_name, errno);
        }

        char only_file_name[128];
        strncpy(only_file_name, &(file_name[file_name_len-7]), 7);
        char abs_file_name_char[MAXPATHLEN];
        sprintf(abs_file_name_char, "%s/%s", abs_dir_name, only_file_name);


        std::string abs_file_name = abs_file_name_char;
        if(emaster_files.find(abs_file_name) != emaster_files.end())
        {
            sprintf(error, "Already opened file %s", abs_file_name_char);
            throw(Rcpp::exception(error));
            return 0;
        }

        FILE* emaster = fopen(file_name, "rb");
        if(NULL == emaster)
        {
            sprintf(error, "Could not open emaster file: %s, errno: %d", file_name, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        emaster_header header;
        if(!read_emaster_header(&header, emaster))
        {
            sprintf(error, "Could not read header of file %s. Errno: %d", file_name, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        int num_files_read = 0;
        emaster_record rec;
        rec.symbol[0] = '\0';
        rec.short_name[0] = '\0';
        rec.long_name[0] = '\0';
        while(read_emaster_record(&rec, emaster))
        {
            ++num_files_read;

            if(rec.symbol[0] != '\0')
            {
                MSDataFile data_file;
                data_file.symbol = rec.symbol;

                if(0 == strlen(rec.long_name))
                {
                    data_file.name = rec.short_name;
                }
                else
                {
                    data_file.name = rec.long_name;
                }

                char full_file_name[MAXPATHLEN];
                sprintf(full_file_name, "%s/F%d.dat", abs_dir_name, rec.f_number);
                data_file.file_name = full_file_name;

                data_file.num_fields     = rec.tot_fields;
                data_file.flag[0]        = rec.flag;
                data_file.flag[1]        = '\0';
                data_file.periodicity[0] = rec.periodicity;
                data_file.periodicity[1] = '\0';

                long int first_yyyymmdd = lroundf(rec.first_date_ms)+19000000;
                data_file.first_date = yyyymmdd_to_date(first_yyyymmdd);

                long int last_yyyymmdd  = lroundf(rec.last_date_ms)+19000000;
                data_file.last_date = yyyymmdd_to_date(last_yyyymmdd);

                if(symbol_map.find(data_file.symbol) == symbol_map.end())
                {
                    symbol_map[data_file.symbol] = data_file;
                }
                if(name_map.find(data_file.name) == name_map.end())
                {
                    name_map[data_file.name] = data_file;
                }

            }

            rec.symbol[0] = '\0';
            rec.short_name[0] = '\0';
            rec.long_name[0] = '\0';
 
        }

        if(num_files_read != header.num_files)
        {
            sprintf(error,
                    "Header says %d files but found %d records in file %s.",
                    header.num_files, num_files_read, file_name);
            throw(Rcpp::exception(error));
            return num_files_read;
        }
        emaster_files.insert(abs_file_name);

        return 0;
    };


    std::set< std::string > get_emaster_files()
    {
        return emaster_files;
    }


    struct MSDataFile
    {
        std::string symbol;
        std::string name;

        std::string file_name;  // File containing data

        int num_fields;         // Number of 4 byte data fields
        char flag[2];           // Either ' '  or '*'
        char periodicity[2];    // 'D', 'W', 'M' etc

        Rcpp::Date first_date;   // First date on which there is data
        Rcpp::Date last_date;    // Last date on which there is data
    };

    std::vector< MSDataFile > get_assets()
    {
        std::vector< MSDataFile > assets;
        std::map< std::string , MSDataFile >::iterator itr     = symbol_map.begin();
        std::map< std::string , MSDataFile >::iterator end_itr = symbol_map.end();
        for(; itr != end_itr; ++itr)
        {
            assets.push_back(itr->second);
        }
        return assets;
    };

    bool get_asset_by_ticker(const std::string &ticker, MSDataFile& data_file)
    {
        std::map< std::string , MSDataFile >::iterator itr =
            symbol_map.find(ticker);
        if(itr != symbol_map.end())
        {
            data_file = itr->second;
            return true;
        }
        return false;
    };

    bool get_asset_by_name(const std::string &name, MSDataFile& data_file)
    {
        std::map< std::string , MSDataFile >::iterator itr = name_map.find(name);
        if(itr != name_map.end())
        {
            data_file = itr->second;
            return true;
        }
        return false;
    };

    struct MS7FieldRec
    {
        Rcpp::Date date;
        float open;
        float high;
        float low;
        float close;
        float volume;
        float open_interest;
    };

    int get_7_field_data(const char* file_name, std::vector< NorgateDirectory::MS7FieldRec > &asset_data)
    {
        asset_data.clear();

        char error[1024];
        FILE* data_file = fopen(file_name, "rb");
        if(NULL == data_file)
        {
            sprintf(error, "Could not open data file: %s, errno: %d", file_name, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        ms_7_field_header header;
        if(!read_7_field_header(&header, data_file))
        {
            sprintf(error, "Could not read header of file %s. Errno: %d", file_name, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        int num_records_read = 0;
        ms_7_field_record rec;

        while(read_7_field_rec(&rec, data_file))
        {
            MS7FieldRec obs;

            obs.date          = ieee_float_to_date(fmsbin2ieee(rec.date));
            obs.open          = fmsbin2ieee(rec.open);
            obs.high          = fmsbin2ieee(rec.high);
            obs.low           = fmsbin2ieee(rec.low);
            obs.close         = fmsbin2ieee(rec.close);
            obs.volume        = fmsbin2ieee(rec.volume);
            obs.open_interest = fmsbin2ieee(rec.open_interest);
            asset_data.push_back(obs);
            ++num_records_read;

        }

        if(num_records_read != (header.last_record-1))
        {
            sprintf(error,
                    "Header says (%d, %d) files. Actually read %d records in file %s.",
                    header.total_records, header.last_record, num_records_read, file_name);
            throw(Rcpp::exception(error));
            return num_records_read;
        }
        return 0;
    };
 




private:

    struct ms_7_field_header
    {
        unsigned short total_records;
        unsigned short last_record;
    };
    bool read_7_field_header(ms_7_field_header *header, FILE *in_stream)
    {

        char junk[28];
        bool good_read =
            ((fread(&header->total_records, sizeof(header->total_records), 1, in_stream) == 1) &&
             (fread(&header->last_record,   sizeof(header->last_record),   1, in_stream) == 1) &&
             (fread(&(junk[0]),             sizeof(junk[0]),              24, in_stream) == 24));
        return good_read;
    };

    struct ms_7_field_record
    {
        unsigned char date[4];
        unsigned char open[4];
        unsigned char high[4];
        unsigned char low[4];
        unsigned char close[4];
        unsigned char volume[4];
        unsigned char open_interest[4];
    };
    bool read_7_field_rec(ms_7_field_record* rec, FILE *in_stream)
    {
        bool good_read =
           ((fread(&(rec->date[0]),          sizeof(rec->date[0]),          4, in_stream) == 4) &&
            (fread(&(rec->open[0]),          sizeof(rec->open[0]),          4, in_stream) == 4) &&
            (fread(&(rec->high[0]),          sizeof(rec->high[0]),          4, in_stream) == 4) &&
            (fread(&(rec->low[0]),           sizeof(rec->low[0]),           4, in_stream) == 4) &&
            (fread(&(rec->close[0]),         sizeof(rec->close[0]),         4, in_stream) == 4) &&
            (fread(&(rec->volume[0]),        sizeof(rec->volume[0]),        4, in_stream) == 4) &&
            (fread(&(rec->open_interest[0]), sizeof(rec->open_interest[0]), 4, in_stream) == 4));
        return good_read;
    };


    // Structure representing the header of an emaster file
    struct emaster_header
    {
        unsigned short num_files;
        unsigned short max_fnum; 
    };
    // Read one emaster header into the *header pointer from in_stream
    bool read_emaster_header(emaster_header *header, FILE* in_stream)
    {
        char junk[192];
        bool good_read =
           ((fread(&(header->num_files), sizeof(header->num_files), 1, in_stream) == 1) &&
            (fread(&(header->max_fnum),  sizeof(header->num_files), 1, in_stream) == 1) &&
            (fread(&(junk[0]),           sizeof(junk[0]),         188, in_stream) == 188));
        return good_read;

    };

    // Structure representing each record in an emaster file
    struct emaster_record
    {
        unsigned char f_number;
        unsigned char tot_fields;
        char flag;
        char symbol[21];
        char short_name[28];
        char periodicity;
        float first_date_ms;
        float last_date_ms;
        char long_name[53];
    };
    // Read one emaster record into the rec pointer reading from in_stream
    bool read_emaster_record(emaster_record *rec, FILE* in_stream)
    {

        char junk[192];
        bool good_read =
            ((fread(&(junk[0]),            sizeof(junk[0]),            2,  in_stream) == 2)  &&
             (fread(&(rec->f_number),      sizeof(rec->f_number),      1,  in_stream) == 1)  &&
             (fread(&(junk[0]),            sizeof(junk[0]),            3,  in_stream) == 3)  && 
             (fread(&(rec->tot_fields),    sizeof(rec->tot_fields),    1,  in_stream) == 1)  &&
             (fread(&(junk[0]),            sizeof(junk[0]),            2,  in_stream) == 2)  &&
             (fread(&(rec->flag),          sizeof(rec->flag),          1,  in_stream) == 1)  &&
             (fread(&(junk[0]),            sizeof(junk[0]),            1,  in_stream) == 1)  &&
             (fread(&(rec->symbol[0]),     sizeof(rec->symbol[0]),     21, in_stream) == 21) &&
             (fread(&(rec->short_name[0]), sizeof(rec->short_name[0]), 28, in_stream) == 28) &&
             (fread(&(rec->periodicity),   sizeof(rec->periodicity),   1,  in_stream) == 1)  &&
             (fread(&(junk[0]),            sizeof(junk[0]),            3,  in_stream) == 3)  &&
             (fread(&(rec->first_date_ms), sizeof(rec->first_date_ms), 1,  in_stream) == 1)  &&
             (fread(&(junk[0]),            sizeof(junk[0]),            4,  in_stream) == 4)  &&
             (fread(&(rec->last_date_ms),  sizeof(rec->last_date_ms),  1,  in_stream) == 1)  &&
             (fread(&(junk[0]),            sizeof(junk[0]),            63, in_stream) == 63) &&
             (fread(&(rec->long_name[0]),  sizeof(rec->short_name[0]), 53, in_stream) == 53));
        return good_read;
    };


    void reset_refdata()
    {;
        symbol_map.clear();
        name_map.clear();
        emaster_files.clear();
    };

    std::map< std::string , MSDataFile > symbol_map;
    std::map< std::string , MSDataFile > name_map;
    std::set< std::string > emaster_files;



    // Get the absolute path from a file descriptor refering to a directory
    // Also needs a file descriptor for the working directory of the process
    // at the time the function is called because it cd's to the directory
    // and calls getwd as opposed to manually following parent directories all
    // the way to the root
    // dir: File descriptor for the directory
    // orig_dir: File descriptor for the current working dir
    // abs_path: a pointer that will be filled in with the absolute path
    int get_absolute_path(const char *dir_name, char *abs_path)
    {
        char error[1024];

        int cur_dir = open(".", O_RDONLY);
        if(cur_dir < 0)
        {
            sprintf(error, "Could not open current directory. Errno: %d.", errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        int data_dir = open(dir_name, O_RDONLY);
        if(data_dir < 0)
        {
            close(cur_dir);
            sprintf(error, "Cannot open directory %s. Errno: %d.", dir_name, errno);
            throw(Rcpp::exception(error));
            return errno;

        }

        if(0 != fchdir(data_dir))
        {
            close(cur_dir);
            close(data_dir);
            sprintf(error, "Error on fchdir to %s.  Errno: %d", dir_name, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        if(getwd(abs_path) == NULL)
        {
            close(cur_dir);
            close(data_dir);
            sprintf(error, "Error on getwd, %s, Errno: %d", abs_path, errno);
            throw(Rcpp::exception(error));
            return errno;
        }

        if(0 != fchdir(cur_dir))
        {
            close(cur_dir);
            close(data_dir);
            sprintf(error, "Error on fchdir.  Errno: %d", errno);
            throw(Rcpp::exception(error));
            return errno;
        }
        close(cur_dir);
        close(data_dir);

        return 0;
    };


    float fmsbin2ieee(const unsigned char *msbin)
    {
        /* MS Binary Format                         */
        /* byte order =>    m3 | m2 | m1 | exponent */
        /* m1 is most significant byte => sbbb|bbbb */
        /* m3 is the least significant byte         */
        /*      m = mantissa byte                   */
        /*      s = sign bit                        */
        /*      b = bit                             */

        /* IEEE Single Precision Float Format       */
        /*    m3        m2        m1     exponent   */
        /* mmmm|mmmm mmmm|mmmm emmm|mmmm seee|eeee  */
        /*          s = sign bit                    */
        /*          e = exponent bit                */
        /*          m = mantissa bit                */

        float result;
        unsigned char *ieee =    (unsigned char *) &result;
        unsigned char  sign     = 0x00;
        unsigned char  ieee_exp = 0x00;
        int i;

        for(i = 0; i < 4; i++)
        {
            ieee[i] = 0;
        }

        /* any msbin w/ exponent of zero = zero */
        if(msbin[3] == 0)
        {
            return 0;
        }

        sign = msbin[2] & 0x80;      /* 1000|0000b  */
        ieee[3] |= sign;

        /* MBF is bias 128 and IEEE is bias 127. ALSO, MBF places   */
        /* the decimal point before the assumed bit, while          */
        /* IEEE places the decimal point after the assumed bit.     */

        /* actually, msbin[3]-1-128+127 */
        ieee_exp = msbin[3] - 2;

        /* the first 7 bits of the exponent in ieee[3] */
        ieee[3] |= ieee_exp >> 1;

        /* the one remaining bit in first bin of ieee[2] */
        ieee[2] |= ieee_exp << 7;

        /* 0111|1111b : mask out the msbin sign bit */
        ieee[2] |= msbin[2] & 0x7f;
        ieee[1] = msbin[1];
        ieee[0] = msbin[0];

        return (result);
    };

    Rcpp::Date ieee_float_to_date(float date_float)
    {
        long int yyyymmdd = lroundf(date_float)+19000000;
        return yyyymmdd_to_date(yyyymmdd);
    };


    Rcpp::Date yyyymmdd_to_date(long yyyymmdd)
    {
        int yyyy = yyyymmdd/10000;
        int mm   = (yyyymmdd - (yyyy*10000))/100;
        int dd   = yyyymmdd % 100;
        return Rcpp::Date(mm, dd, yyyy);
    };

};


RcppExport SEXP createNorgateReader()
{
    BEGIN_RCPP;

    NorgateDirectory *ngt_dir = new NorgateDirectory();
    Rcpp::XPtr<  NorgateDirectory > rcpp_norgate(ngt_dir, true);

    return rcpp_norgate;
    END_RCPP;
};


RcppExport SEXP open_directory(SEXP rcpp_norgate_ptr, SEXP rcpp_dir_name)
{
    BEGIN_RCPP
    Rcpp::XPtr< NorgateDirectory > rcpp_norgate(rcpp_norgate_ptr);


    Rcpp::IntegerMatrix ret(1);
    const std::string dir_name = Rcpp::as< std::string >(rcpp_dir_name);
    ret[0] = rcpp_norgate->add_directory(dir_name.c_str());

    return ret;
    END_RCPP;
};


RcppExport SEXP open_emaster(SEXP rcpp_norgate_ptr, SEXP rcpp_emaster_name)
{
    BEGIN_RCPP
    Rcpp::XPtr< NorgateDirectory > rcpp_norgate(rcpp_norgate_ptr);

    Rcpp::IntegerMatrix ret(1);
    const std::string emaster_name = Rcpp::as< std::string >(rcpp_emaster_name);
    ret[0] = rcpp_norgate->add_emaster_file(emaster_name.c_str());

    return ret;
    END_RCPP;
};


RcppExport SEXP get_emasters(SEXP rcpp_norgate_ptr)
{
    BEGIN_RCPP;
    Rcpp::XPtr< NorgateDirectory > rcpp_norgate(rcpp_norgate_ptr);
    std::set< std::string > em_names = rcpp_norgate->get_emaster_files();
    Rcpp::CharacterVector rcpp_em_names(em_names.begin(), em_names.end());

    return rcpp_em_names;
    END_RCPP;
};


RcppExport SEXP available_assets(SEXP rcpp_norgate_ptr)
{
    BEGIN_RCPP;
    Rcpp::XPtr< NorgateDirectory > rcpp_norgate(rcpp_norgate_ptr);

    std::vector< NorgateDirectory::MSDataFile > data_files =
        rcpp_norgate->get_assets();
    const size_t num_files = data_files.size();

    Rcpp::CharacterVector symbols(num_files);
    Rcpp::CharacterVector asset_names(num_files);
    Rcpp::CharacterVector file_names(num_files);
    Rcpp::IntegerVector   num_fields(num_files);
    Rcpp::CharacterVector flag(num_files);
    Rcpp::CharacterVector periodicity(num_files);
    Rcpp::DateVector      first_date(num_files);
    Rcpp::DateVector      last_date(num_files);

    std::vector< NorgateDirectory::MSDataFile >::iterator itr = data_files.begin();
    std::vector< NorgateDirectory::MSDataFile >::iterator end_itr = data_files.end();
    size_t idx = 0;
    for(; itr != end_itr; ++itr)
    {
        symbols[idx]     = itr->symbol;
        asset_names[idx] = itr->name;
        file_names[idx]  = itr->file_name;
        num_fields[idx]  = itr->num_fields;
        flag[idx]        = itr->flag;
        periodicity[idx] = itr->periodicity;
        first_date[idx]  = itr->first_date;
        last_date[idx]   = itr->last_date;
        ++idx;
    }

    Rcpp::DataFrame df = 
        Rcpp::DataFrame::create(Rcpp::Named("ticker")           = symbols,
                                Rcpp::Named("name")             = asset_names,
                                Rcpp::Named("data.file")        = file_names,
                                Rcpp::Named("num.fields")       = num_fields,
                                Rcpp::Named("flag")             = flag,
                                Rcpp::Named("periodicity")      = periodicity,
                                Rcpp::Named("start.date")       = first_date,
                                Rcpp::Named("end.date")         = last_date,
                                Rcpp::Named("stringsAsFactors") = false);

    df.attr("row.names") = symbols;

    return df;

    END_RCPP;
};


RcppExport SEXP get_data_from_file(SEXP rcpp_norgate_ptr, SEXP rcpp_data_filename)
{
    BEGIN_RCPP;
    char error[1024];
    Rcpp::XPtr< NorgateDirectory > rcpp_norgate(rcpp_norgate_ptr);

    std::string data_filename = Rcpp::as< std::string >(rcpp_data_filename);
    std::vector< NorgateDirectory::MS7FieldRec > asset_data;
    int read_stat =
        rcpp_norgate->get_7_field_data(data_filename.c_str(), asset_data);
    if(0 != read_stat)
    {
        sprintf(error, "Could not read data in file %s", data_filename.c_str());
        throw(Rcpp::exception(error));
    }

    size_t num_obs = asset_data.size();

    Rcpp::DateVector    date(num_obs);
    Rcpp::NumericVector open(num_obs);
    Rcpp::NumericVector high(num_obs);
    Rcpp::NumericVector low(num_obs);
    Rcpp::NumericVector close(num_obs);
    Rcpp::NumericVector volume(num_obs);
    Rcpp::NumericVector open_interest(num_obs);

    std::vector< NorgateDirectory::MS7FieldRec >::iterator itr = asset_data.begin();
    std::vector< NorgateDirectory::MS7FieldRec >::iterator end_itr = asset_data.end();
    size_t idx = 0;
    for(; itr != end_itr; ++itr)
    {
        date[idx]           = itr->date;
        open[idx]           = double(itr->open);
        high[idx]           = double(itr->high);
        low[idx]            = double(itr->low);
        close[idx]          = double(itr->close);
        volume[idx]         = double(itr->volume);
        open_interest[idx]  = double(itr->open_interest);
        ++idx;
    }

    Rcpp::DataFrame df = Rcpp::DataFrame::create(
            Rcpp::Named("tradedate")        = date,
            Rcpp::Named("open")             = open,
            Rcpp::Named("high")             = high,
            Rcpp::Named("low")              = low,
            Rcpp::Named("close")            = close,
            Rcpp::Named("volume")           = volume,
            Rcpp::Named("open.interest")    = open_interest);


    Rcpp::CharacterVector df_rownames;
    Rcpp::DateVector::iterator date_itr = date.begin();
    Rcpp::DateVector::iterator end_date_itr = date.end();
    for(; date_itr != end_date_itr; ++date_itr)
    {
        char date_name[64];
        sprintf(date_name,
                "%.4d-%.2d-%.2d",
                date_itr->getYear(), date_itr->getMonth(), date_itr->getDay());
        df_rownames.push_back(Rcpp::String(date_name));
    }

    df.attr("row.names") = df_rownames;

    return df;
    END_RCPP;
};


