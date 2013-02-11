#include <Rcpp.h>

#include <string>
#include <map>
#include <set>
#include <cstdio>

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/param.h>


void stop(const char *error)
{
    printf("%s\n", error);
}




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
            stop(error);
            return fs_stat;
        }
        for(auto itr = emasters_found.begin(); itr != emasters_found.end(); ++itr)
        {
            int add_stat = add_emaster_file((itr->c_str()));
            if(0 != add_stat)
            {
                sprintf(error, "Could not read emaster file %s, Errno: %d",
                        itr->c_str(), errno);
                stop(error);
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
            stop(error);
            return errno;
        }

        DIR  *dp = opendir(abs_dir_name);
        if(NULL == dp)
        {
            sprintf(error, "Cannot open directory: %s, Errno: %d.", dir_name, errno);
            stop(error);
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
                        stop(error);
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
            stop(error);
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
            stop(error);
            return 0;
        }

        FILE* emaster = fopen(file_name, "rb");
        if(NULL == emaster)
        {
            sprintf(error, "Could not open emaster file: %s, errno: %d", file_name, errno);
            stop(error);
            return errno;
        }

        emaster_header header;
        if(!read_emaster_header(&header, emaster))
        {
            sprintf(error, "Could not read header of file %s. Errno: %d", file_name, errno);
            stop(error);
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

                data_file.num_fields = rec.tot_fields;
                data_file.flag = rec.flag;
                data_file.periodicity = rec.periodicity;

                //rec.first_date_ms,
                //rec.last_date_ms,

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
            stop(error);
            return num_files_read;
        }
        emaster_files.insert(abs_file_name);

        return 0;
    };



private:


    struct MSDataFile
    {
        std::string symbol;
        std::string name;

        std::string file_name;

        int num_fields;             // Number of 4 byte data fields
        char flag;                  // Either ' '  or '*'
        char periodicity;           // 'D', 'W', 'M' etc

        //date firstDate;             // First date on which there is data
        //date lastDate;              // Last date on which there is data
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
            (fread(&(header->max_fnum), sizeof(header->num_files), 1, in_stream)  == 1) &&
            (fread(&(junk[0]), sizeof(junk[0]), 188, in_stream)                   == 188));
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
    std::map< std::string , MSDataFile> name_map;
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
            stop(error);
            return errno;
        }

        int data_dir = open(dir_name, O_RDONLY);
        if(data_dir < 0)
        {
            close(cur_dir);
            sprintf(error, "Cannot open directory %s. Errno: %d.", dir_name, errno);
            stop(error);
            return errno;

        }

        if(0 != fchdir(data_dir))
        {
            close(cur_dir);
            close(data_dir);
            sprintf(error, "Error on fchdir to %s.  Errno: %d", dir_name, errno);
            stop(error);
            return errno;
        }

        if(getwd(abs_path) == NULL)
        {
            close(cur_dir);
            close(data_dir);
            sprintf(error, "Error on getwd, %s, Errno: %d", abs_path, errno);
            stop(error);
            return errno;
        }

        if(0 != fchdir(cur_dir))
        {
            close(cur_dir);
            close(data_dir);
            sprintf(error, "Error on fchdir.  Errno: %d", errno);
            stop(error);
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

};



int main()
{
    NorgateDirectory *nd = new NorgateDirectory();

    /*
    std::set< std::string > em_files;

    int ret_val = nd->find_emaster_files("/Volumes/BigData2/Norgate/Data/Futures", em_files);
    for(auto itr = em_files.begin(); itr != em_files.end(); ++itr)
    {
        printf("%s\n", itr->c_str());
    }
    */

    nd->add_emaster_file("/Volumes/BigData2/Norgate/Data/Forex/emaster");
}


/*
RcppExport SEXP initIndex(SEXP r_training_data, SEXP r_params)
{
    BEGIN_RCPP;

    Rcpp::S4 rcpp_params(r_params);
    const ANNsplitRule split_rule =
        ANNsplitRule(Rcpp::as< int >(rcpp_params.slot("split.rule")));
    const ANNshrinkRule shrink_rule =
        ANNshrinkRule(Rcpp::as< int >(rcpp_params.slot("shrink.rule")));
    const int bucket_size = Rcpp::as< int >(rcpp_params.slot("bucket.size"));


    Rcpp::NumericMatrix rcpp_training_data(r_training_data);
    const size_t num_train = rcpp_training_data.nrow();
    const size_t dimension = rcpp_training_data.ncol();
    //printf("Num train is %zu, Dimension is %zu\n.", num_train, dimension);

    ANNpointArray training_data = annAllocPts(num_train, dimension);
    for(size_t i = 0; i != num_train; ++i)
    {
        for(size_t j = 0; j != dimension; ++j)
        {
            training_data[i][j] = rcpp_training_data(i, j);
        }
    }

    ANNbd_tree *tree = new ANNbd_tree(
        training_data,
        num_train,
        dimension,
        bucket_size,
        split_rule,
        shrink_rule);

    Rcpp::IntegerVector rcpp_ret_val(1);
    if(NULL == tree)
    {
        rcpp_ret_val[0] = -1;
        return Rcpp::List::create(Rcpp::Named("ret.val") = rcpp_ret_val);
    }
    else
    {
        rcpp_ret_val[0] = 0;
    }

    ann_cleanup *cup = new ann_cleanup(tree, training_data);
    Rcpp::XPtr< ann_cleanup > rcpp_index(cup, true);

    return Rcpp::List::create(
            Rcpp::Named("ret.val")       = rcpp_ret_val,
            Rcpp::Named("idx.ptr")       = rcpp_index);

    END_RCPP;
}
*/
