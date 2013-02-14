
setClass(Class = "NorgateData",
    representation = list(
         read.ptr = "externalptr",
         built = "logical",
         emasters = "character",
         assets = "data.frame"),

    prototype = prototype(built=FALSE, emasters=c(), assets=data.frame(),

    validity = function(.Object){
        errors <- list()

        if(!.Object@built){
            errors <- c(errors, "NorgateData not initialized!!")
        }

        if(length(errors)> 0){
            return(errors)
        }
        TRUE
    }
)


setMethod(
    f = "initialize",
    signature = c("NorgateData"),
    definition = function(.Object, dir.names=c(), emaster.names=c()){

        .Object@built <- FALSE

        .Object@read.ptr <- .Call("createNorgateReader")
        .Object@built <- TRUE

        if(length(dir.names > 0)){
            openDirectory(.Object, dir.names)
        }
        if(length(emaster.names) > 0){
            openEMaster(.Object, emaster.names)
        }

        validObject(.Object)
        .Object
    }
)

createNorgateDir <- function(dir.names=c(), emaster.names=c())
{
    new(Class="NorgateData", dir.names=dir.names, emaster.names=emaster.names)
}


setGeneric("openDirectory",
    function(.Object, filenames){
        standardGeneric("openDirectory")
    }
)
setMethod("openDirectory",
    signature = c("NorgateData", "character"),
    definition = function(.Object, directory){
        stopifnot(.Object@built)
        stopifnot(is.character(directory))
        stopifnot(length(directory) > 0)

        dir.executable <- file.access(names=dir.names, mode=1);
        stopifnot(all(0 == dir.executable))

        for(dir.name in dir.names){
            stopifnot(0 == .Call("open_directory", .Object@read.ptr, dir.name))
        }

        .Object@emasters <- .Call("emaster_files", .Object@read.ptr)
        .Object@assets   <- .Call("available_assets", .Object@read.ptr)
    }
)

setGeneric("openEMaster",
    function(.Object, filenames){
        standardGeneric("openEMaster")
    }
)
setMethod("openEMaster",
    signature = c("NorgateData", "character"),
    definition = function(.Object, emaster.names){
        stopifnot(.Object@built)
        stopifnot(is.character(emaster.names))
        stopifnot(length(emaster.names) > 0)

        emaster.readable <- file.access(names=emaster.names, mode=4);
        stopifnot(all(0 == emaster.readable))

        for(emaster.name in emaster.names){
            stopifnot(0 == .Call("open_emaster", .Object@read.ptr, emaster.name))
        }

        .Object@emasters <- .Call("emaster_files", .Object@read.ptr)
        .Object@assets   <- .Call("available_assets", .Object@read.ptr)
    }
)

setGeneric("getDataFromFile",
    function(.Object, filename){
        standardGeneric("getDataFromFile")
    }
)

setMethod("getDataFromFile",
    signature = c("NorgateData", "character"),
    definition = function(.Object, filename){
        stopifnot(.Object@built)
        stopifnot(is.character(filename))
        stopifnot(length(filename)==1)

        data.df <- .Call("get_data_from_file", .Object@idx.ptr, filename)
        data.df
    }
)

setGeneric("getDataByTicker",
    function(.Object, ticker){
        standardGeneric("getDataByTicker")
    }
)

setMethod("getDataByTicker",
    signature = c("NorgateData", "character"),
    definition = function(.Object, ticker){
        stopifnot(.Object@built)
        stopifnot(is.character(ticker))
        stopifnot(length(ticker) == 1)

        data.idx <- which(.Object@assets$ticker == ticker)
        stopifnot(length(data.idx) == 1)
        stopifnot(.Object, .Object@assets$num.fields[data.idx] == 7)
        getDataFromFile(.Object, .Object@assets$data.file[data.idx])
    }
)

setMethod("[",
    signature=c("NorgateData", "character"),
    definition = function(.Object, ticker){
        getDataByTicker(.Object, ticker)
   }
)


setGeneric("getDataByName",
    function(.Object, name){
        standardGeneric("getDataByName")
    }
)

setMethod("getDataByName",
    signature = c("NorgateData", "character"),
    definition = function(.Object, name){
        stopifnot(.Object@built)
        stopifnot(is.character(ticker))
        stopifnot(length(name) == 1)

        data.idx <- which(.Object@assets$name == name)
        stopifnot(length(data.idx) == 1)
        stopifnot(.Object, .Object@assets$num.fields[data.idx] == 7)
        getDataFromFile(.Object, .Object@assets$data.file[data.idx])
    }
)
