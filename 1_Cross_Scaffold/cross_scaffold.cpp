//
// Transfer edited low res stack tforms back to orig tiles.
//


#include	"Cmdline.h"
#include	"Disk.h"
#include	"File.h"
#include	"PipeFiles.h"
#include	"TrakEM2_UTL.h"
#include	"../1_Cross_LowRes/ScapeMeta.h"

using namespace ns_pipergns;


/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Types --------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* CArgs_alnmon -------------------------------------------------- */
/* --------------------------------------------------------------- */

class CArgs_alnmon {

public:
	const char	*srcmons,
				*xml_lowres;
	int			zmin,
				zmax;

public:
	CArgs_alnmon()
	{
		srcmons		= NULL;
		xml_lowres	= "LowRes.xml";
		zmin		= 0;
		zmax		= 32768;
	};

	void SetCmdLine( int argc, char* argv[] );
};

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */

static CArgs_alnmon	gArgs;
static string		idb;
static FILE*		flog = NULL;






/* --------------------------------------------------------------- */
/* SetCmdLine ---------------------------------------------------- */
/* --------------------------------------------------------------- */

void CArgs_alnmon::SetCmdLine( int argc, char* argv[] )
{
// start log

	flog = FileOpenOrDie( "cross_scaffold.log", "w" );

// log start time

	time_t	t0 = time( NULL );
	char	atime[32];

	strcpy( atime, ctime( &t0 ) );
	atime[24] = '\0';	// remove the newline

	fprintf( flog, "Assemble stack: %s ", atime );

// parse command line args

	if( argc < 3 ) {
		printf(
		"Usage: cross_scaffold srcmons -z=i,j [options].\n" );
		exit( 42 );
	}

	vector<int>	vi;

	for( int i = 1; i < argc; ++i ) {

		// echo to log
		fprintf( flog, "%s ", argv[i] );

		if( argv[i][0] != '-' )
			srcmons = argv[i];
		else if( GetArgStr( xml_lowres, "-lowres=", argv[i] ) )
			;
		else if( GetArgList( vi, "-z=", argv[i] ) ) {

			if( 2 == vi.size() ) {
				zmin = vi[0];
				zmax = vi[1];
			}
			else {
				fprintf( flog,
				"Bad format in -z [%s].\n", argv[i] );
				exit( 42 );
			}
		}
		else {
			printf( "Did not understand option '%s'.\n", argv[i] );
			exit( 42 );
		}
	}

	fprintf( flog, "\n\n" );
	fflush( flog );
}

/* --------------------------------------------------------------- */
/* LoadTAffines -------------------------------------------------- */
/* --------------------------------------------------------------- */

static void LoadTAffines( vector<TAffine> &vT )
{
	XML_TKEM		xml( gArgs.xml_lowres, flog );
	TiXmlElement*	layer	= xml.GetFirstLayer();
	int				nz = 0;

	for( ; layer; layer = layer->NextSiblingElement() ) {

		TiXmlElement* p = layer->FirstChildElement( "t2_patch" );

		vT[nz++].ScanTrackEM2( p->Attribute( "transform" ) );
	}
}

/* --------------------------------------------------------------- */
/* UpdateTAffines ------------------------------------------------ */
/* --------------------------------------------------------------- */

static void UpdateTAffines()
{
// Get log data

	vector<CLog>	vL;
	int				nL = ReadLogs( vL, gArgs.zmin, gArgs.zmax );

	if( !nL )
		return;

// Get scaled-down image -> 0-image TForms

	vector<TAffine>	vTs( nL );

	LoadTAffines( vTs );

// Build whole montage TForms

	vector<TAffine>	vTm( nL );

	for( int ia = 1; ia < nL; ++ia ) {

		const CScapeMeta	&Ma = vL[ia].M;
		const CScapeMeta	&M0 = vL[0].M;
		TAffine				R0i, Ra, t, s;

		// A-montage -> A-oriented
		Ra.NUSetRot( Ma.deg*PI/180 );

		// A-oriented -> A-image (like Scape.cpp)
		s.NUSetScl( 1.0/Ma.scl );
		t = s * Ra;
		t.AddXY( -Ma.x0, -Ma.y0 );

		// A-image -> 0-image
		t = vTs[ia] * t;

		// 0-image -> 0-image oriented
		R0i.InverseOf( vTs[0] );
		t = R0i * t;

		// 0-image oriented -> 0-oriented (reverse Scape.cpp)
		t.AddXY( M0.x0, M0.y0 );
		s.NUSetScl( M0.scl );
		t = s * t;

		// 0-oriented -> 0-montage
		R0i.NUSetRot( -M0.deg*PI/180 );
		vTm[ia] = R0i * t;
	}

// Apply

	char	out[128], buf[2048];
	bool	isbin;

	// create destination

	isbin = (strstr( FileNamePtr( gArgs.srcmons ), "X_A_BIN" )
			 != NULL);

	if( isbin )
		strcpy( out, "X_A_BIN_scaf" );
	else
		strcpy( out, "X_A_TXT_scaf" );

	DskCreateDir( out, flog );

	// copy data for lowest layer

	sprintf( buf, "cp %s/*_%d* %s", gArgs.srcmons, gArgs.zmin, out );
	system( buf );

	// load and modify above layers

	for( int z = gArgs.zmin + 1; z <= gArgs.zmax; ++z ) {

		Rgns	R;
		int		ib;

		if( !R.Init( idb, z, flog ) )
			continue;

		if( !R.Load( gArgs.srcmons ) )
			continue;

		// find TAffine for this z

		for( ib = 0; ib < nL; ++ib ) {

			if( vL[ib].M.z == z )
				break;
		}

		if( ib >= nL )
			continue;

		map<int,int>::iterator	mi, en = R.m.end();

		for( mi = R.m.begin(); mi != en; ) {

			int	id		= mi->first,
				j0		= mi->second,
				jlim	= (++mi == en ? R.nr : mi->second);

			for( int j = j0; j < jlim; ++j ) {

				if( !FLAG_ISUSED( R.flag[j] ) )
					continue;

				TAffine	&T = X_AS_AFF( R.x, j );

				T = vTm[ib] * T;
			}
		}

		if( isbin )
			R.SaveBIN( out, true );
		else
			R.SaveTXT( out );
	}
}

/* --------------------------------------------------------------- */
/* main ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

int main( int argc, char* argv[] )
{
/* ------------------ */
/* Parse command line */
/* ------------------ */

	gArgs.SetCmdLine( argc, argv );

	IDBFromTemp( idb, "..", flog );

	if( idb.empty() )
		exit( 42 );

/* ------------- */
/* Update TForms */
/* ------------- */

	UpdateTAffines();

/* ---- */
/* Done */
/* ---- */

exit:
	fprintf( flog, "\n" );
	fclose( flog );

	return 0;
}


