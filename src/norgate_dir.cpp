//#include <Rcpp.h>

#include <string>
#include <map>
#include <set>
#include <cstdio>

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/param.h>


void stop(char *error)
{
    printf("%s\n", error);
}




struct DataFileRep
{
    std::string symbol;
    std::string name;

    int fileNum;        // The file name is F<fileNum>
    int numFld;         // Number of 4 byte data fields
    char flag;          // Either ' '  or '*'
    char periodicity;   // 'D', 'W', 'M' etc

    //date firstDate;     // First date on which there is data
    //date lastDate;      // Last date on which there is data
};


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

    int find_emaster_files(char *dirName, std::set< std::string > &emasters)
    {
        char error[1024];

        int cur_dir = open(".", O_RDONLY);
        if(cur_dir < 0)
        {
            sprintf(error, "Could not open current directory. Errno: %d.", errno);
            stop(error);
            return errno;
        }


        char data_dir_name[MAXPATHLEN];
        int data_dir = open(dirName, O_RDONLY);
        if(data_dir < 0)
        {
            sprintf(error, "Cannot open directory %s. Errno: %d.", dirName, errno);
            stop(error);
            return errno;

        }
        if(get_absolute_path(data_dir, cur_dir, data_dir_name))
        {
            sprintf(error, "Error in get_absolute_path");
            stop(error);
            return errno;
        }
        close(data_dir);

        DIR  *dp = opendir(dirName);
        if(NULL == dp)
        {
            sprintf(error, "Cannot open directory: %s, Errno: %d.", dirName, errno);
            stop(error);
            return errno;
        }

        dirent *dirp;
        while((dirp = readdir(dp)) != NULL)
        {
            char full_path[MAXPATHLEN];
            strcpy(full_path, data_dir_name);
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
        if(0 != fchdir(cur_dir))
        {
            sprintf(error, "Error on fchdir.  Errno: %d.", errno);
            stop(error);
            return errno;
        }
        close(cur_dir);
        return 0;
    };

    int add_directory(std::string dirName)
    {
        return 0;
    };

    int add_emaster_file(char *fileName)
    {
        char error[1024];
        FILE* emaster = fopen(fileName, "rb");
        if(NULL == emaster)
        {
            sprintf(error, "Could not open file: %s, errno: %d", fileName, errno);
            stop(error);
            return errno;
        }

        printf("%s\n", "Reading emaster header.");
        emaster_header header;
        if(!read_emaster_header(&header, emaster))
        {
            sprintf(error, "Could not read header of file %s. Errno: %d", fileName, errno);
            stop(error);
            return errno;
        }
        printf("%s\n", "Done reading emaster header.");
        char output_line[1024];
        sprintf(output_line, "%hu, %hu\n", header.num_files, header.max_fnum);
        printf("%s", output_line);

        int num_files_read = 0;
        emaster_record rec;
        while(read_emaster_record(&rec, emaster))
        {
            ++num_files_read;

            /*
            float first_date, last_date;
            if(fmsbintoieee(&(rec.first_date_ms), &first_date) != 0)
            {
                sprintf(error, "Could not convert first date to IEEE float.");
                stop(error);
                return -1;
            }
            if(fmsbintoieee(&(rec.last_date_ms), &last_date) != 0)
            {
                sprintf(error, "Could not convert last date to IEEE float.");
                stop(error);
                return -1;
            }
            */



            sprintf(
                output_line, 
                "FNum: %hhu, TotFld: %hhu, Flag: %c, Sym: %s, SNm: %s, Prd: %c, FD: %f, LD: %f, LNm: %s\n\n\n\n",
                rec.f_number,
                rec.tot_fields,
                rec.flag,
                rec.symbol,
                rec.short_name,
                rec.periodicity,
                rec.first_date_ms,
                rec.last_date_ms,
                rec.long_name);
            printf("%s", output_line);
        }
        printf("Read %d lines.\n", num_files_read);
        printf("Expection %d records with max_fnum = %d.\n",
               header.num_files, header.max_fnum);

        return 0;
    };



private:

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
        symbolMap.clear();
        nameMap.clear();
        emaster_files.clear();
    };

    std::map< std::string , DataFileRep > symbolMap;
    std::map< std::string , DataFileRep > nameMap;
    std::set< std::string > emaster_files;



    // Get the absolute path from a file descriptor refering to a directory
    // Also needs a file descriptor for the working directory of the process
    // at the time the function is called because it cd's to the directory
    // and calls getwd as opposed to manually following parent directories all
    // the way to the root
    // dir: File descriptor for the directory
    // orig_dir: File descriptor for the current working dir
    // abs_path: a pointer that will be filled in with the absolute path
    int get_absolute_path(int dir, int orig_dir, char *abs_path)
    {
        char error[1024];

        if(0 != fchdir(dir))
        {
            sprintf(error, "Error on fchdir.  Errno: %d", errno);
            stop(error);
            return errno;
        }

        if(getwd(abs_path) == NULL)
        {
            sprintf(error, "Error on getwd, %s, Errno: %d", abs_path, errno);
            stop(error);
            return errno;
        }

        if(0 != fchdir(orig_dir))
        {
            sprintf(error, "Error on fchdir.  Errno: %d", errno);
            stop(error);
            return errno;
        }
        return 0;
    };


    int fmsbintoieee(float *src4, float *dest4)
    {
        unsigned char *msbin = (unsigned char *)src4;
        unsigned char *ieee = (unsigned char *)dest4;
        unsigned char sign = 0x00;
        unsigned char ieee_exp = 0x00;
        int i;

        /* MS Binary Format                         */
        /* byte order =>    m3 | m2 | m1 | exponent */
        /* m1 is most significant byte => sbbb|bbbb */
        /* m3 is the least significant byte         */
        /*      m = mantissa byte                   */
        /*      s = sign bit                        */
        /*      b = bit                             */

        sign = msbin[2] & 0x80;      /* 1000|0000b  */

        /* IEEE Single Precision Float Format       */
        /*    m3        m2        m1     exponent   */
        /* mmmm|mmmm mmmm|mmmm emmm|mmmm seee|eeee  */
        /*          s = sign bit                    */
        /*          e = exponent bit                */
        /*          m = mantissa bit                */

        for (i=0; i<4; i++)
        {
            ieee[i] = 0;
        }

        /* any msbin w/ exponent of zero = zero */
        if (msbin[3] == 0)
        {
            return 0;
        }

        ieee[3] |= sign;

        /* MBF is bias 128 and IEEE is bias 127. ALSO, MBF places   */
        /* the decimal point before the assumed bit, while          */
        /* IEEE places the decimal point after the assumed bit.     */

        ieee_exp = msbin[3] - 2;    /* actually, msbin[3]-1-128+127 */

        /* the first 7 bits of the exponent in ieee[3] */
        ieee[3] |= ieee_exp >> 1;

        /* the one remaining bit in first bin of ieee[2] */
        ieee[2] |= ieee_exp << 7;

        /* 0111|1111b : mask out the msbin sign bit */
        ieee[2] |= msbin[2] & 0x7f;

        ieee[1] = msbin[1];
        ieee[0] = msbin[0];

        return 0;
    }

    float BasicToIEEE(unsigned char *value)
    {
        float result;
        unsigned char *msbin =   (unsigned char *)  value;
        unsigned char *ieee =    (unsigned char *) &result;
        unsigned char  sign     = 0x00;
        unsigned char  ieee_exp = 0x00;
        int i;
        /* MS Binary Format                         */
        /* byte order =>    m3 | m2 | m1 | exponent */
        /* m1 is most significant byte => sbbb|bbbb */
        /* m3 is the least significant byte         */
        /*      m = mantissa byte                   */
        /*      s = sign bit                        */
        /*      b = bit                             */
        sign = msbin[2] & 0x80;      /* 1000|0000b  */
        /* IEEE Single Precision Float Format       */
        /*    m3        m2        m1     exponent   */
        /* mmmm|mmmm mmmm|mmmm emmm|mmmm seee|eeee  */
        /*          s = sign bit                    */
        /*          e = exponent bit                */
        /*          m = mantissa bit                */
        for (i=0; i<4; i++) ieee[i] = 0;
        /* any msbin w/ exponent of zero = zero */
        if (msbin[3] == 0) return 0;
        ieee[3] |= sign;
        /* MBF is bias 128 and IEEE is bias 127. ALSO, MBF places   */
        /* the decimal point before the assumed bit, while          */
        /* IEEE places the decimal point after the assumed bit.     */
        ieee_exp = msbin[3] - 2;    /* actually, msbin[3]-1-128+127 */
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
    std::set< std::string > em_files;

    int ret_val = nd->find_emaster_files("/Volumes/BigData2/Norgate/Data/Futures", em_files);
    for(auto itr = em_files.begin(); itr != em_files.end(); ++itr)
    {
        printf("%s\n", itr->c_str());
    }

    printf("Sizeof unsigned short = %zu\n", sizeof(unsigned short));
    printf("Sizeof int = %zu\n", sizeof(int));

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
