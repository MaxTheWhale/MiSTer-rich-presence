target_sources(mister-status
    PRIVATE
        mister_status.c
        error.c
        network.c
        helpers.c
    PRIVATE FILE_SET HEADERS FILES
        error.h
        helpers.h
        network.h
)
