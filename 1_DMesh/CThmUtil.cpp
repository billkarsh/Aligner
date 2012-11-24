

#include	"CThmUtil.h"
#include	"Disk.h"
#include	"Maths.h"
#include	"Geometry.h"
#include	"Debug.h"






/* --------------------------------------------------------------- */
/* Echo ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::Echo( vector<TForm> &guesses )
{
	guesses.push_back( Tab );
	return true;
}

/* --------------------------------------------------------------- */
/* FromLog ------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::FromLog( vector<TForm> &guesses )
{
	ThmPair	tpr;
	int		ok;

	ok = ReadThmPair( tpr,
			A.layer, A.tile, aid,
			B.layer, B.tile, bid, flog )
		&& !tpr.err;

	if( ok )
		guesses.push_back( tpr.T );

	return ok;
}

/* --------------------------------------------------------------- */
/* SetStartingAngle ---------------------------------------------- */
/* --------------------------------------------------------------- */

// Set starting angle (ang0) and use it to adjust OLAP2D_XL.
//
// Return {0=use denovo method, n=count of prior angles}.
//
// Notes
// =====
//
// All modes use ang0 as the dbgCor angle and the center angle.
// In all cases, Tab, the starting guess for the sought rigid
// transform, is adjusted so that Tab, written as R+V, becomes
// R(ang0)+V.
//
// Generally, Tab is used from the idb, unless it was overridden
// from the command line as -Tab=. Further, if the command line
// specifies -CTR= then that overrides all. MODE=M and MODE=Z
// implicitly set -CTR=0.
//
// MODE=Y is slightly different, wherein prior angles are read
// from a table. If the number of prior angles is < 4 we are in
// a denovo ang0 discovery phase and the hierarchy described
// above applies. However, if nprior is >= 4, then ang0 is set
// to the median of those angles.
//
int CThmUtil::SetStartingAngle( double CTR )
{
	double			Vx = Tab.t[2],
					Vy = Tab.t[5];
	vector<ThmPair>	tpr;
	int				ntpr, nprior = 0;

// Handle modes N, C (includes M, Z)

	if( MODE == 'N' || MODE == 'C' ) {

		if( CTR != 999.0 ) {
			ang0 = CTR;
			Tab.NUSetRot( CTR * PI/180.0 );
			Tab.SetXY( Vx, Vy );
		}
		else
			ang0 = 180.0/PI * RadiansFromAffine( Tab );

		nprior = 1;
		goto adjust_olap;
	}

// Handle mode Y: Try to get prior angles

	if( ReadAllThmPair( tpr, A.layer, B.layer, flog )
		&& (ntpr = tpr.size()) ) {

		vector<double>	A;

		for( int i = 0; i < ntpr; ++i ) {

			if( !tpr[i].err )
				A.push_back( tpr[i].A );
		}

		if( (nprior = A.size()) >= 4 )
			ang0 = MedianVal( A );
		else
			nprior = 0;
	}

// Otherwise guess for denovo case

	if( !nprior ) {

		if( CTR != 999.0 )
			ang0 = CTR;
		else
			ang0 = 180.0/PI * RadiansFromAffine( Tab );
	}

// Force tiny ang0 to zero

	if( fabs( ang0 ) < 0.001 )
		ang0 = 0.0;

// Tab becomes R(ang0) + V

	Tab.NUSetRot( ang0 * PI/180.0 );
	Tab.SetXY( Vx, Vy );

// Adjust OLAP2D_XL

adjust_olap:
	if( A.layer != B.layer ) {

		double	a = ang0 * PI/180.0,
				c = cos( a ),
				s = sin( a );

		OLAP2D = long(OLAP2D / fmax( c*c, s*s ));
	}

	return nprior;
}

/* --------------------------------------------------------------- */
/* SubI_ThesePoints ---------------------------------------------- */
/* --------------------------------------------------------------- */

void CThmUtil::SubI_ThesePoints(
	SubI					&S,
	const vector<double>	&v,
	const vector<Point>		&p )
{
	IBox	B;
	int		w	= px.ws,
			np	= p.size();

	BBoxFromPoints( B, p );

	S.v.resize( np );
	S.p.resize( np );

	for( int i = 0; i < np; ++i ) {

		const Point&	P = p[i];

		S.v[i]		= v[int(P.x) + w * int(P.y)];
		S.p[i].x	= P.x - B.L;
		S.p[i].y	= P.y - B.B;
	}

	S.O	= Point( B.L, B.B );
	S.w = B.R - B.L + 1;
	S.h = B.T - B.B + 1;
}

/* --------------------------------------------------------------- */
/* SubI_ThisBox -------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::SubI_ThisBox(
	SubI					&S,
	const vector<double>	&v,
	const vector<Point>		&p,
	const IBox				&Bolap )
{
// Select points in box

	int	np = p.size();

	for( int i = 0; i < np; ++i ) {

		const Point&	P = p[i];

		if( P.x >= Bolap.L && P.x <= Bolap.R &&
			P.y >= Bolap.B && P.y <= Bolap.T ) {

			S.p.push_back( P );
		}
	}

// Enough points?

	if( S.p.size() <= OLAP2D )
		return false;

// Set SubI

	SubI_ThesePoints( S, v, S.p );

	return true;
}

/* --------------------------------------------------------------- */
/* Olap_WholeImage ----------------------------------------------- */
/* --------------------------------------------------------------- */

// Set fields of olp appropriate for whole imaging searching.
//
// Return true always as convenience.
//
bool CThmUtil::Olap_WholeImage(
	OlapRec				&olp,
	const vector<Point>	&apts,
	const vector<Point>	&bpts )
{
	fprintf( flog,
	"Subimage: Using whole images, apix=%d, bpix=%d\n",
	apts.size(), bpts.size() );

	SubI_ThesePoints( olp.a, *px.avs_aln, apts );
	SubI_ThesePoints( olp.b, *px.bvs_aln, bpts );

	return true;
}

/* --------------------------------------------------------------- */
/* Olap_TheseBoxes_NoCR ------------------------------------------ */
/* --------------------------------------------------------------- */

bool CThmUtil::Olap_TheseBoxes_NoCR(
	OlapRec		&olp,
	const IBox	&Ba,
	const IBox	&Bb )
{
	int	WW = px.ws,
		ow = Ba.R - Ba.L + 1,
		oh = Ba.T - Ba.B + 1,
		np = ow * oh;

	fprintf( flog,
	"Subimage: Using intersection, w=%d, h=%d, pix=%d\n",
	ow, oh, np );

// Points

	MakeZeroBasedPoints( olp.a.p, ow, oh );
	olp.b.p = olp.a.p;

// Values

	const vector<double>&	av = *px.avs_aln;
	const vector<double>&	bv = *px.bvs_aln;

	olp.a.v.resize( np );
	olp.b.v.resize( np );

	for( int i = 0; i < np; ++i ) {

		int	y = i / ow;
		int	x = i - ow * y;

		olp.a.v[i] = av[Ba.L + x + WW*(Ba.B + y)];
		olp.b.v[i] = bv[Bb.L + x + WW*(Bb.B + y)];
	}

// Dimensions

	olp.a.O	= Point( Ba.L, Ba.B );
	olp.b.O	= Point( Bb.L, Bb.B );
	olp.a.w = ow;
	olp.b.w = ow;
	olp.a.h = oh;
	olp.b.h = oh;

	return true;
}

/* --------------------------------------------------------------- */
/* Olap_WholeImage_NoCR ------------------------------------------ */
/* --------------------------------------------------------------- */

// Set fields of olp appropriate for whole imaging searching.
//
// Return true always as convenience.
//
bool CThmUtil::Olap_WholeImage_NoCR( OlapRec &olp )
{
	int	w = px.ws,
		h = px.hs;

	fprintf( flog,
	"Subimage: Using whole images, pix=%d\n", w * h );

// Values

	olp.a.v = *px.avs_aln;
	olp.b.v = *px.bvs_aln;

// Points

	MakeZeroBasedPoints( olp.a.p, w, h );
	olp.b.p = olp.a.p;

// Dimensions

	olp.a.O	= Point( 0, 0 );
	olp.b.O	= olp.a.O;
	olp.a.w = w;
	olp.b.w = w;
	olp.a.h = h;
	olp.b.h = h;

	return true;
}

/* --------------------------------------------------------------- */
/* GetOlapBoxes -------------------------------------------------- */
/* --------------------------------------------------------------- */

void CThmUtil::GetOlapBoxes( IBox &Ba, IBox &Bb, double XYCONF )
{
// Get displacements of a in b-system

	int	dx, dy;

	{
		Point	XY;

		Tab.Transform( XY );

		dx = int(XYCONF * XY.x / px.scl);
		dy = int(XYCONF * XY.y / px.scl);
	}

// Use offsets and image size to determine
// {Ba, Bb} overlap boxes.

	int	w = px.ws,
		h = px.hs;

	BoxesFromShifts( Ba, Bb, w, h, w, h, dx, dy );
}

/* --------------------------------------------------------------- */
/* Crop ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

// For FFT efficiency, crop the images as closely as possible
// to the likely intersection areas.
//
// Return true if non-empty olap.
//
bool CThmUtil::Crop(
	OlapRec				&olp,
	const ConnRegion	&acr,
	const ConnRegion	&bcr,
	double				XYCONF )
{
// Use whole images if no confidence

	if( !XYCONF )
		return Olap_WholeImage( olp, acr.pts, bcr.pts );

// Get overlap boxes

	IBox	Ba, Bb;

	GetOlapBoxes( Ba, Bb, XYCONF );

// Test if sufficient overlap

	int	ow		= Ba.R - Ba.L + 1,
		oh		= Ba.T - Ba.B + 1,
		min1d	= max( OLAP1D, 8 );

	if( ow < min1d || oh < min1d ) {
		fprintf( flog, "Subimage: 1D overlap too small.\n" );
		return Olap_WholeImage( olp, acr.pts, bcr.pts );
	}

// Set using overlap

	fprintf( flog,
	"Subimage: Using intersection, w=%d, h=%d, pix=%d\n",
	ow, oh, ow * oh );

	if( !SubI_ThisBox( olp.a, *px.avs_aln, acr.pts, Ba ) ||
		!SubI_ThisBox( olp.b, *px.bvs_aln, bcr.pts, Bb ) ) {

		fprintf( flog, "Subimage: 2D overlap too small.\n" );
		return false;
	}

	return true;
}

/* --------------------------------------------------------------- */
/* Crop_NoCR ----------------------------------------------------- */
/* --------------------------------------------------------------- */

// For FFT efficiency, crop the images as closely as possible
// to the likely intersection areas.
//
// Return true always.
//
bool CThmUtil::Crop_NoCR( OlapRec &olp, double XYCONF )
{
// Use whole images if no confidence

	if( !XYCONF )
		return Olap_WholeImage_NoCR( olp );

// Get overlap boxes

	IBox	Ba, Bb;

	GetOlapBoxes( Ba, Bb, XYCONF );

// Test if sufficient overlap

	int	ow		= Ba.R - Ba.L + 1,
		oh		= Ba.T - Ba.B + 1,
		min1d	= max( OLAP1D, 8 );

	if( ow < min1d || oh < min1d ) {
		fprintf( flog, "Subimage: 1D overlap too small.\n" );
		return Olap_WholeImage_NoCR( olp );
	}

// Set using overlap

	return Olap_TheseBoxes_NoCR( olp, Ba, Bb );
}

/* --------------------------------------------------------------- */
/* MakeThumbs ---------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::MakeThumbs(
	ThmRec			&thm,
	const OlapRec	&olp,
	int				decfactor )
{
	thm.av		= olp.a.v;
	thm.bv		= olp.b.v;
	thm.ap		= olp.a.p;
	thm.bp		= olp.b.p;
	thm.ftc.clear();
	thm.reqArea	= OLAP2D;
	thm.olap1D	= OLAP1D;
	thm.scl		= decfactor;

	if( decfactor > 1 ) {

		DecimateVector( thm.ap, thm.av, olp.a.w, olp.a.h, decfactor );
		DecimateVector( thm.bp, thm.bv, olp.b.w, olp.b.h, decfactor );

		thm.reqArea	/= decfactor * decfactor;
		thm.olap1D  /= decfactor;

		fprintf( flog,
		"Thumbs: After decimation %d pts, reqArea %d, thmscl %d\n",
		thm.ap.size(), thm.reqArea, thm.scl );
	}

	double	sd = Normalize( thm.av );

	if( !sd || !isfinite( sd ) ) {

		fprintf( flog,
		"Thumbs: Image A intersection region has stdev: %f\n", sd );

		return false;
	}

	sd = Normalize( thm.bv );

	if( !sd || !isfinite( sd ) ) {

		fprintf( flog,
		"Thumbs: Image B intersection region has stdev: %f\n", sd );

		return false;
	}

	return true;
}

/* --------------------------------------------------------------- */
/* Disc ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::Disc(
	CorRec			&best,
	CThmScan		&S,
	ThmRec			&thm,
	const OlapRec	&olp,
	int				PRETWEAK )
{
	Point	delta, TaO = olp.a.O;
	int		Ox, Oy, Rx;

	Tab.Transform( delta );
	Tab.Apply_R_Part( TaO );

	Ox = ROUND((delta.x / px.scl - olp.b.O.x + TaO.x) / thm.scl);
	Oy = ROUND((delta.y / px.scl - olp.b.O.y + TaO.y) / thm.scl);
	Rx = LIMXY / (px.scl * thm.scl);

	fprintf( flog, "SetDisc( %d, %d, %d, %d )\n", Ox, Oy, Rx, Rx );

	S.SetUseCorrR( true );
	S.SetDisc( Ox, Oy, Rx, Rx );
	S.RFromAngle( best, ang0, thm );

	fprintf( flog,
	"Initial: R=%6.3f, A=%8.3f, X=%8.2f, Y=%8.2f\n",
	best.R, best.A, best.X, best.Y );

	if( dbgCor )
		return false;

	if( best.R < RTRSH ) {

		if( PRETWEAK ) {

			S.Pretweaks( best.R, ang0, thm );
			S.RFromAngle( best, ang0, thm );

			fprintf( flog,
			"Tweaked: R=%6.3f, A=%8.3f, X=%8.2f, Y=%8.2f\n",
			best.R, best.A, best.X, best.Y );
		}

		if( best.R < RTRSH ) {

			fprintf( flog,
			"FAIL: Approx: MODE=N R=%g below thresh=%g\n",
			best.R, RTRSH );

			return Failure( best, errLowRPrior );
		}
	}

	Point	dS(
			(best.X - Ox) * (thm.scl * px.scl),
			(best.Y - Oy) * (thm.scl * px.scl) );

	fprintf( flog,
	"Peak-Disc: dR %d dX %d dY %d\n",
	int(sqrt( dS.RSqr() )), int(dS.x), int(dS.y) );

	return true;
}

/* --------------------------------------------------------------- */
/* DebugSweepKill ------------------------------------------------ */
/* --------------------------------------------------------------- */

void CThmUtil::DebugSweepKill( CThmScan &S, ThmRec thm )
{
	const double	center	= ang0;
	const double	hlfwid	= HFANGPR;
	const double	step	= 0.1;

	//const double	hlfwid	= 1.0;
	//const double	step	= 0.01;

	S.DebugAngs( A.layer, A.tile, B.layer, B.tile,
		center, hlfwid, step, thm );

	exit( 42 );
}

/* --------------------------------------------------------------- */
/* Sweep --------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::Sweep(
	CorRec		&best,
	CThmScan	&S,
	ThmRec		&thm,
	int			nPrior )
{
// Debug and exit

	//DebugSweepKill( S, thm );

	if( dbgCor ) {
		S.RFromAngle( best, ang0, thm );
		return false;
	}

// Sweep

	if( nPrior ) {

		if( !S.UsePriorAngles( best, nPrior, ang0, HFANGPR, thm ) )
			return Failure( best, S.GetErr() );
	}
	else if( !S.DenovoBestAngle( best, ang0, HFANGDN, 0.5, thm ) )
		return Failure( best, S.GetErr() );

	return true;
}

/* --------------------------------------------------------------- */
/* IsectToImageCoords -------------------------------------------- */
/* --------------------------------------------------------------- */

// Let T(A) -> B be resolved into parts: R(A) + V -> B.
// Thus far we have found R,V for points {a', b'} relative
// to their respective intersection origins, aO, bO. So we
// have found:
//
//		R( a' = a - aO ) + V -> b' = b - bO.
//
// We can rearrange this to:
//
//		R( a ) + ( V + bO - R(aO) ) -> b,
//
// which shows directly how to translate V to the image corner.
//
void CThmUtil::IsectToImageCoords(
	CorRec		&best,
	Point		aO,
	const Point	&bO )
{
	best.T.Apply_R_Part( aO );

	best.X += bO.x - aO.x;
	best.Y += bO.y - aO.y;

	best.T.SetXY( best.X, best.Y );
}

/* --------------------------------------------------------------- */
/* RecordSumSqDif ------------------------------------------------ */
/* --------------------------------------------------------------- */

// ******************************
// ALL CALCS USING SCALED PX DATA
// ******************************

static void SQD(
	double			&sqd,
	double			&prd,
	int				&N,
	const PixPair	&px,
	const TForm		&T )
{
	sqd	= 0.0;
	prd	= 0.0;
	N	= 0;

	const vector<double>&	av = *px.avs_vfy;
	const vector<double>&	bv = *px.bvs_vfy;

	vector<Point>	ap, bp;
	int				w  = px.ws,
					h  = px.hs,
					np = w * h;

// Fill points

	MakeZeroBasedPoints( ap, w, h );
	bp = ap;

// Sums

	for( int i = 0; i < np; ++i ) {

		Point	p = ap[i];

		T.Transform( p );

		if( p.x >= 0.0 && p.x < w-1 &&
			p.y >= 0.0 && p.y < h-1 ) {

			double	d = InterpolatePixel( p.x, p.y, bv, w );

			++N;
			prd += av[i] * d;
			d   -= av[i];
			sqd += d * d;
		}
	}
}


void CThmUtil::RecordSumSqDif( const TForm &T )
{
	CMutex	M;
	char	name[256];

	sprintf( name, "sqd_%d_%d", A.layer, B.layer );

	if( M.Get( name ) ) {

		sprintf( name, "SmSqDf_%d_@_%d.log", A.layer, B.layer );

		FILE *f;
		int  is;

		f = fopen( name, "r" );
		if( is = (f != NULL) )
			fclose( f );

		f = fopen( name, "a" );

		if( f ) {

			if( !is )
				fprintf( f, "TileA\tTileB\tSQ\tR\tN\tSQ/N\tR/N\n" );

			double	sqd, prd;
			int		N;

			SQD( sqd, prd, N, px, T );

			fprintf( f, "%d\t%d\t%f\t%f\t%d\t%f\t%f\n",
				A.tile, B.tile, sqd, prd, N, sqd/N, prd/N );
			fflush( f );
			fclose( f );
		}
	}

	M.Release();
}

/* --------------------------------------------------------------- */
/* FullScaleReportToLog ------------------------------------------ */
/* --------------------------------------------------------------- */

void CThmUtil::FullScaleReportToLog( CorRec &best )
{
	best.T.MulXY( px.scl );

	fprintf( flog, "Approx: Returning A=%f, R=%f, X=%f, Y=%f\n",
	best.A, best.R, best.T.t[2], best.T.t[5] );

	fprintf( flog, "Approx: Best transform " );
	best.T.PrintTransform( flog );
}

/* --------------------------------------------------------------- */
/* Check_LIMXY --------------------------------------------------- */
/* --------------------------------------------------------------- */

// Always report on the difference between found TForm and Tab.
//
// Iff LIMXY != 0 and iff using an angle sweep mode, then
// return false if the difference exceeds LIMXY.
//
bool CThmUtil::Check_LIMXY( const TForm &Tbest )
{
	TForm	Tinv, I;

// Always report

	fprintf( flog, "Approx: Orig transform " );
	Tab.PrintTransform( flog );

	InvertTrans( Tinv, Tab );
	MultiplyTrans( I, Tinv, Tbest );
	fprintf( flog, "Approx: Idnt transform " );
	I.PrintTransform( flog );

	double	err = sqrt( I.t[2]*I.t[2] + I.t[5]*I.t[5] );

	fprintf( flog, "Approx: err = %g, max = %d\n", err, LIMXY );

// Optional rejection test

	if( LIMXY && MODE != 'N' && err > LIMXY ) {

		fprintf( flog,
		"FAIL: Approx: Too different from Tab err=%g, max=%d\n",
		err, LIMXY );

		return false;
	}

	return true;
}

/* --------------------------------------------------------------- */
/* TabulateResult ------------------------------------------------ */
/* --------------------------------------------------------------- */

void CThmUtil::TabulateResult( const CorRec &best, int err )
{
	ThmPair	tpr;

	tpr.T	= best.T;
	tpr.A	= best.A;
	tpr.R	= best.R;
	tpr.err	= err;

	WriteThmPair( tpr, A.layer, A.tile, aid, B.layer, B.tile, bid );
}

/* --------------------------------------------------------------- */
/* Failure ------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CThmUtil::Failure( const CorRec &best, int err )
{
	TabulateResult( best, err );
	return false;
}

/* --------------------------------------------------------------- */
/* Finish -------------------------------------------------------- */
/* --------------------------------------------------------------- */

// - Do post-tweaks; mostly makes sense for EM.
// - Undo thumb scaling.
// - Refine at px.scl ("full scale").
// - Translate from intersection to image coords.
// - Optionally make sum of square difs report.
// - Undo px.scl scaling.
// - Always report final result to log.
// - Check result against LIMXY.
// - Tabulate.
//
// Return true if LIMXY passed.
//
bool CThmUtil::Finish(
	CorRec			&best,
	CThmScan		&S,
	ThmRec			&thm,
	const OlapRec	&olp,
	int				TWEAKS )
{
// Tweaks

	if( TWEAKS )
		S.PostTweaks( best, thm );

// Undo thumb scaling

	best.X *= thm.scl;
	best.Y *= thm.scl;

// Full resolution (px.scl) adjustment

	if( !MakeThumbs( thm, olp, 1 ) )
		return false;

	S.FinishAtFullRes( best, thm );

// Translate coords

	IsectToImageCoords( best, olp.a.O, olp.b.O );

// Make Reports

	//RecordSumSqDif( best.T );

	FullScaleReportToLog( best );

// Sanity check translation

	if( !Check_LIMXY( best.T ) )
		return false;

// Tabulate

	TabulateResult( best, S.GetErr() );

	return true;
}

