/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/*
 * ireg - The irods reg utility
 */

#include "rodsClient.h"
#include "parseCommandLine.h"
#include "rodsPath.h"
#include "regUtil.h"
#include "irods_client_api_table.hpp"
#include "irods_pack_table.hpp"
#include "irods_database_plugin_cockroachdb_structs.hpp"
#include "avro/Encoder.hh"
#include "avro/Decoder.hh"
#include "avro/ValidSchema.hh"
#include "avro/Compiler.hh"
#include "procApiRequest.h"
#include <string>
#include <fstream>
#include <streambuf>

void usage();

int
main( int argc, char **argv ) {

    signal( SIGPIPE, SIG_IGN );

    int status;
    rodsEnv myEnv;
    rErrMsg_t errMsg;
    rcComm_t *conn;
    rodsArguments_t myRodsArgs;
    char *optStr;
    int nArgv;


    optStr = "h";

    status = parseCmdLineOpt( argc, argv, optStr, 1, &myRodsArgs );

    if ( status < 0 ) {
        printf( "use -h for help.\n" );
        exit( 1 );
    }

    if ( myRodsArgs.help == True ) {
        usage();
        exit( 0 );
    }

    nArgv = argc - optind;

    if ( nArgv != 1 ) {    /* must have 2 inputs */
        usage();
        exit( 1 );
    }

    status = getRodsEnv( &myEnv );
    if ( status < 0 ) {
        rodsLogError( LOG_ERROR, status, "main: getRodsEnv error. " );
        exit( 1 );
    }

    std::ifstream inschema("/var/lib/irods/avro_schemas/irods_database_plugin_cockroachdb_structs.json");
    avro::ValidSchema cpxSchema;
    avro::compileJsonSchema(inschema, cpxSchema);
    auto in = avro::fileInputStream(argv[optind]);
    auto dec = avro::jsonDecoder(cpxSchema);

    dec->init(*in);
    irods::Bulk bulk;
    avro::decode(*dec, bulk);

    auto out = avro::memoryOutputStream();
    auto enc = avro::binaryEncoder();
    enc->init(*out);
    avro::encode(*enc, bulk);
    enc->flush();
    out->flush();
    size_t bc = out->byteCount();
    auto in2 = avro::memoryInputStream(*out);
    avro::StreamReader r(*in2);
    bytesBuf_t bb;
    bb.len = bc;
    bb.buf = new uint8_t[bc];
    r.readBytes(reinterpret_cast<uint8_t*>(bb.buf), bb.len);




    // =-=-=-=-=-=-=-
    // initialize pluggable api table
    irods::api_entry_table&  api_tbl = irods::get_client_api_table();
    irods::pack_entry_table& pk_tbl  = irods::get_pack_table();
    init_api_table( api_tbl, pk_tbl );

    conn = rcConnect( myEnv.rodsHost, myEnv.rodsPort, myEnv.rodsUserName,
                      myEnv.rodsZone, 1, &errMsg );

    if ( conn == NULL ) {
        exit( 2 );
    }

    status = clientLogin( conn );
    if ( status != 0 ) {
        rcDisconnect( conn );
        exit( 7 );
    }

    void *output;
    status = procApiRequest(conn, 30000, &bb, NULL, &output, NULL);

    printErrorStack( conn->rError );
    rcDisconnect( conn );
    delete[] (reinterpret_cast<uint8_t*>(bb.buf));

    if ( status < 0 ) {
        exit( 3 );
    }
    else {
        exit( 0 );
    }

}

void
usage() {
    char *msgs[] = {
        "Usage: ibulkreg inputFilePath",
        ""
    };
    int i;
    for ( i = 0;; i++ ) {
        if ( strlen( msgs[i] ) == 0 ) {
            break;
        }
        printf( "%s\n", msgs[i] );
    }
    printReleaseInfo( "ibulkreg" );
}
