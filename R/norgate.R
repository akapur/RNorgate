
setClass(Class = "NorgateData",
    representation = list(
         read.ptr = "externalptr",
         built = "logical",
         emasters = "character",
         assets = "data.frame"),

    prototype = prototype(built=FALSE, emasters=character(0), assets=data.frame()),

    validity = function(object){
        errors <- list()

        if(!object@built){
            errors <- c(errors, "NorgateData not initialized!!")
        }

        if(length(object@emasters) < 1){
            errors <- c(errors, "No emaster files are open.")
        }

        if(nrow(object@assets) < 1){
            errors <- c(errors, "We don't have data for any assets.")
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
    definition = function(.Object, dir.names, emaster.names){

        missing.dir.names     <- missing(dir.names)
        missing.emaster.names <- missing(emaster.names)
        stopifnot(!(missing.dir.names & missing.emaster.names))

        .Object@read.ptr <- .Call("createNorgateReader")

        if(!missing.dir.names){
            stopifnot(is.character(dir.names))
            dir.executable = file.access(names=dir.names, mode=1)
            stopifnot(all(0 == dir.executable))
            for(dir.name in dir.names){
                stopifnot(0 == .Call("open_directory", .Object@read.ptr, dir.name))
            }
        }
        if(!missing.emaster.names){
            stopifnot(is.character(emaster.names))
            emaster.readable <- file.access(names=emaster.names, mode=4);
            stopifnot(all(0 == emaster.readable))
            for(emaster.name in emaster.names){
                stopifnot(0 == .Call("open_emaster", .Object@read.ptr, emaster.name))
            }
        }
        .Object@emasters <- .Call("get_emasters", .Object@read.ptr)
        .Object@assets   <- .Call("available_assets", .Object@read.ptr)
        .Object@built <- TRUE

        validObject(.Object)
        .Object
    }
)

createNorgate <- function(dir.names, emaster.names)
{
    missing.dir.names <- missing(dir.names)
    missing.emaster.names <- missing(emaster.names)

    stopifnot(!(missing.dir.names & missing.emaster.names))
    if(missing.emaster.names & (!missing.dir.names)){
        nd <- new(Class="NorgateData", dir.names=dir.names)
    }
    if(missing.dir.names & (!missing.emaster.names)){
        nd <- new(Class="NorgateData", emaster.names=emaster.names)
    }
    if((!missing.dir.names) & (!missing.emaster.names)){
        nd <- new(Class="NorgateData", dir.names=dir.names, emaster.names=emaster.names)
    }
    nd
}


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

        .Call("get_data_from_file", .Object@read.ptr, filename)
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
        stopifnot(.Object@assets$num.fields[data.idx] == 7)
        getDataFromFile(.Object, .Object@assets$data.file[data.idx])
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
        stopifnot(.Object@assets$num.fields[data.idx] == 7)
        getDataFromFile(.Object, .Object@assets$data.file[data.idx])
    }
)


setGeneric("assets",
    function(.Object){
        standardGeneric("assets")
    }
)

setMethod("assets",
    signature = c("NorgateData"),
    definition = function(.Object){
        stopifnot(.Object@built)
        .Object@assets
    }
)


setGeneric("emasters",
    function(.Object){
        standardGeneric("emasters")
    }
)
setMethod("emasters",
    signature = c("NorgateData"),
    definition = function(.Object){
        stopifnot(.Object@built)
        .Object@emasters
    }
)




setMethod("[",
    signature = c(x="NorgateData", i="character", j="missing", drop="missing"),
    definition = function(x, i){
        getDataByTicker(x, i)
   }
)

setMethod("show",
    signature = c("NorgateData"),
    definition = function(object){
        rep.str <- sprintf("RNorgate Object with data on %d assets from %d files.\n",
                           nrow(object@assets), length(object@emasters))
        cat(rep.str)
    }
)


