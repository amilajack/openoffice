/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_xmloff.hxx"
#include <com/sun/star/util/DateTime.hpp>
#include <com/sun/star/util/Date.hpp>
#include <com/sun/star/util/Time.hpp>
#include <tools/debug.hxx>
#include <rtl/ustrbuf.hxx>
#include "xmlehelp.hxx"
#include <xmloff/xmlement.hxx>
#include <xmloff/xmluconv.hxx>
#include <xmloff/xmltoken.hxx>
#include <rtl/math.hxx>
#include <rtl/logfile.hxx>

#ifndef _TOOLS_DATE_HXX
#include <tools/date.hxx>

#include <tools/string.hxx>

#endif

#include <tools/time.hxx>
#include <tools/fldunit.hxx>

// #110680#
//#ifndef _COMPHELPER_PROCESSFACTORY_HXX_
//#include <comphelper/processfactory.hxx>
//#endif
#include <com/sun/star/util/XNumberFormatsSupplier.hpp>
#include <com/sun/star/style/NumberingType.hpp>
#include <com/sun/star/text/XNumberingTypeInfo.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/i18n/XCharacterClassification.hpp>
#include <com/sun/star/i18n/UnicodeType.hpp>
#include <basegfx/vector/b3dvector.hxx>

using namespace rtl;
using namespace com::sun::star;
using namespace com::sun::star::uno;
using namespace com::sun::star::lang;
using namespace com::sun::star::text;
using namespace com::sun::star::style;
using namespace ::com::sun::star::i18n;
using namespace ::xmloff::token;

const sal_Int8 XML_MAXDIGITSCOUNT_TIME = 11;
const sal_Int8 XML_MAXDIGITSCOUNT_DATETIME = 6;
#define XML_NULLDATE "NullDate"

OUString SvXMLUnitConverter::msXML_true;
OUString SvXMLUnitConverter::msXML_false;

void SvXMLUnitConverter::initXMLStrings()
{
    if( msXML_true.getLength() == 0 )
    {
        msXML_true = GetXMLToken(XML_TRUE);
        msXML_false = GetXMLToken(XML_FALSE);
    }
}

void SvXMLUnitConverter::createNumTypeInfo() const
{
	// #110680#
    //Reference< lang::XMultiServiceFactory > xServiceFactory =
    //        comphelper::getProcessServiceFactory();
    //OSL_ENSURE( xServiceFactory.is(),
    //        "XMLUnitConverter: got no service factory" );

	if( mxServiceFactory.is() )
    {
        ((SvXMLUnitConverter *)this)->xNumTypeInfo =
            Reference < XNumberingTypeInfo > (
                mxServiceFactory->createInstance(
                    OUString(RTL_CONSTASCII_USTRINGPARAM("com.sun.star.text.DefaultNumberingProvider") ) ), UNO_QUERY );
    }
}

/** constructs a SvXMLUnitConverter. The core measure unit is the
    default unit for numerical measures, the XML measure unit is
    the default unit for textual measures
*/

// #110680#
//SvXMLUnitConverter::SvXMLUnitConverter( MapUnit eCoreMeasureUnit,
//                                        MapUnit eXMLMeasureUnit ) :
SvXMLUnitConverter::SvXMLUnitConverter(
	MapUnit eCoreMeasureUnit,
	MapUnit eXMLMeasureUnit,
	const uno::Reference<lang::XMultiServiceFactory>& xServiceFactory ) :
    aNullDate(30, 12, 1899),
	mxServiceFactory( xServiceFactory )
{
	DBG_ASSERT( mxServiceFactory.is(), "got no service manager" );

	meCoreMeasureUnit = eCoreMeasureUnit;
    meXMLMeasureUnit = eXMLMeasureUnit;
}

SvXMLUnitConverter::~SvXMLUnitConverter()
{
}

MapUnit SvXMLUnitConverter::GetMapUnit(sal_Int16 nFieldUnit)
{
    MapUnit eUnit = MAP_INCH;
    switch( nFieldUnit )
    {
    case FUNIT_MM:
        eUnit = MAP_MM;
        break;
    case FUNIT_CM:
    case FUNIT_M:
    case FUNIT_KM:
        eUnit = MAP_CM;
        break;
    case FUNIT_TWIP:
        eUnit = MAP_TWIP;
        break;
    case FUNIT_POINT:
    case FUNIT_PICA:
        eUnit = MAP_POINT;
        break;
//  case FUNIT_INCH:
//  case FUNIT_FOOT:
//  case FUNIT_MILE:
//      eUnit = MAP_INCH;
//      break;
    case FUNIT_100TH_MM:
        eUnit = MAP_100TH_MM;
        break;
    }
    return eUnit;
}

/** convert string to measure using optional min and max values*/
sal_Bool SvXMLUnitConverter::convertMeasure( sal_Int32& nValue,
                                         const OUString& rString,
                                         sal_Int32 nMin, sal_Int32 nMax ) const
{
    return SvXMLUnitConverter::convertMeasure( nValue, rString,
                                               meCoreMeasureUnit,
                                               nMin, nMax );
}

/** convert measure to string */
void SvXMLUnitConverter::convertMeasure( OUStringBuffer& rString,
                                         sal_Int32 nMeasure ) const
{
    SvXMLUnitConverter::convertMeasure( rString, nMeasure,
                                        meCoreMeasureUnit,
                                        meXMLMeasureUnit );
}

/** convert measure with given unit to string */
void SvXMLUnitConverter::convertMeasure( OUStringBuffer& rString,
                                         sal_Int32 nMeasure,
                                         MapUnit eSrcUnit ) const
{
    SvXMLUnitConverter::convertMeasure( rString, nMeasure,
                                        eSrcUnit,
                                        meXMLMeasureUnit );
}

/** convert the value from the given string to an int value
    with the given map unit using optional min and max values
*/
sal_Bool SvXMLUnitConverter::convertMeasure( sal_Int32& rValue,
                                         const OUString& rString,
                                         MapUnit eDstUnit,
                                         sal_Int32 nMin, sal_Int32 nMax )
{
    sal_Bool bNeg = sal_False;
    double nVal = 0;

    sal_Int32 nPos = 0;
    const sal_Int32 nLen = rString.getLength();

    // skip white space
    while( (nPos < nLen) && (rString[nPos] <= sal_Unicode(' ')) )
        nPos++;

    if( nPos < nLen && sal_Unicode('-') == rString[nPos] )
    {
        bNeg = sal_True;
        ++nPos;
    }

    // get number
    while( nPos < nLen &&
           sal_Unicode('0') <= rString[nPos] &&
           sal_Unicode('9') >= rString[nPos] )
    {
        // TODO: check overflow!
        nVal *= 10;
        nVal += (rString[nPos] - sal_Unicode('0'));
        ++nPos;
    }
    double nDiv = 1.;
    if( nPos < nLen && sal_Unicode('.') == rString[nPos] )
    {
        ++nPos;

        while( nPos < nLen &&
               sal_Unicode('0') <= rString[nPos] &&
               sal_Unicode('9') >= rString[nPos] )
        {
            // TODO: check overflow!
            nDiv *= 10;
            nVal += ( ((double)(rString[nPos] - sal_Unicode('0'))) / nDiv );
            ++nPos;
        }
    }

    // skip white space
    while( (nPos < nLen) && (rString[nPos] <= sal_Unicode(' ')) )
        ++nPos;

    if( nPos < nLen )
    {

        if( MAP_RELATIVE == eDstUnit )
        {
            if( sal_Unicode('%') != rString[nPos] )
                return sal_False;
        }
        else if( MAP_PIXEL == eDstUnit )
        {
            if( nPos + 1 >= nLen ||
                (sal_Unicode('p') != rString[nPos] &&
                 sal_Unicode('P') != rString[nPos])||
                (sal_Unicode('x') != rString[nPos+1] &&
                 sal_Unicode('X') != rString[nPos+1]) )
                return sal_False;
        }
        else
        {
            DBG_ASSERT( MAP_TWIP == eDstUnit || MAP_POINT == eDstUnit ||
                        MAP_100TH_MM == eDstUnit || MAP_10TH_MM == eDstUnit, "unit is not supported");
            const sal_Char *aCmpsL[2] = { 0, 0 };
            const sal_Char *aCmpsU[2] = { 0, 0 };
            double aScales[2] = { 1., 1. };

            if( MAP_TWIP == eDstUnit )
            {
                switch( rString[nPos] )
                {
                case sal_Unicode('c'):
                case sal_Unicode('C'):
                    aCmpsL[0] = "cm";
                    aCmpsU[0] = "CM";
                    aScales[0] = (72.*20.)/2.54; // twip
                    break;
                case sal_Unicode('e'):
                case sal_Unicode('E'):
        //          pCmp1 = sXML_unit_em;
        //          nToken1 = CSS1_EMS;

        //          pCmp2 = sXML_unit_ex;
        //          nToken2 = CSS1_EMX;
                    break;
                case sal_Unicode('i'):
                case sal_Unicode('I'):
                    aCmpsL[0] = "in";
                    aCmpsU[0] = "IN";
                    aScales[0] = 72.*20.; // twip
                    break;
                case sal_Unicode('m'):
                case sal_Unicode('M'):
                    aCmpsL[0] = "mm";
                    aCmpsU[0] = "MM";
                    aScales[0] = (72.*20.)/25.4; // twip
                    break;
                case sal_Unicode('p'):
                case sal_Unicode('P'):
                    aCmpsL[0] = "pt";
                    aCmpsU[0] = "PT";
                    aScales[0] = 20.; // twip

                    aCmpsL[1] = "pc";
                    aCmpsU[1] = "PC";
                    aScales[1] = 12.*20.; // twip

        //          pCmp3 = sXML_unit_px;
        //          nToken3 = CSS1_PIXLENGTH;
                    break;
                }
            }
            else if( MAP_100TH_MM == eDstUnit || MAP_10TH_MM == eDstUnit )
            {
				double nScaleFactor = (MAP_100TH_MM == eDstUnit) ? 100.0 : 10.0;
                switch( rString[nPos] )
                {
                case sal_Unicode('c'):
                case sal_Unicode('C'):
                    aCmpsL[0] = "cm";
                    aCmpsU[0] = "CM";
                    aScales[0] = 10.0 * nScaleFactor; // mm/100
                    break;
                case sal_Unicode('e'):
                case sal_Unicode('E'):
        //          pCmp1 = sXML_unit_em;
        //          nToken1 = CSS1_EMS;

        //          pCmp2 = sXML_unit_ex;
        //          nToken2 = CSS1_EMX;
                    break;
                case sal_Unicode('i'):
                case sal_Unicode('I'):
                    aCmpsL[0] = "in";
                    aCmpsU[0] = "IN";
                    aScales[0] = 1000.*2.54; // mm/100
                    break;
                case sal_Unicode('m'):
                case sal_Unicode('M'):
                    aCmpsL[0] = "mm";
                    aCmpsU[0] = "MM";
                    aScales[0] = 1.0 * nScaleFactor; // mm/100
                    break;
                case sal_Unicode('p'):
                case sal_Unicode('P'):
                    aCmpsL[0] = "pt";
                    aCmpsU[0] = "PT";
                    aScales[0] = (10.0 * nScaleFactor*2.54)/72.; // mm/100

                    aCmpsL[1] = "pc";
                    aCmpsU[1] = "PC";
                    aScales[1] = (10.0 * nScaleFactor*2.54)/12.; // mm/100

        //          pCmp3 = sXML_unit_px;
        //          nToken3 = CSS1_PIXLENGTH;
                    break;
                }
            }
            else if( MAP_POINT == eDstUnit )
            {
                if( rString[nPos] == 'p' || rString[nPos] == 'P' )
                {
                    aCmpsL[0] = "pt";
                    aCmpsU[0] = "PT";
                    aScales[0] = 1;
                }
            }

            if( aCmpsL[0] == NULL )
                return sal_False;

            double nScale = 0.;
            for( sal_uInt16 i= 0; i < 2; ++i )
            {
                const sal_Char *pL = aCmpsL[i];
                if( pL )
                {
                    const sal_Char *pU = aCmpsU[i];
                    while( nPos < nLen && *pL )
                    {
                        sal_Unicode c = rString[nPos];
                        if( c != *pL && c != *pU )
                            break;
                        ++pL;
                        ++pU;
                        ++nPos;
                    }
                    if( !*pL && (nPos == nLen || ' ' == rString[nPos]) )
                    {
                        nScale = aScales[i];
                        break;
                    }
                }
            }

            if( 0. == nScale )
                return sal_False;

            // TODO: check overflow
            if( nScale != 1. )
                nVal *= nScale;
        }
    }

    nVal += .5;
    if( bNeg )
        nVal = -nVal;

    if( nVal <= (double)nMin )
        rValue = nMin;
    else if( nVal >= (double)nMax )
        rValue = nMax;
    else
        rValue = (sal_Int32)nVal;

    return sal_True;
}

/** convert measure in given unit to string with given unit */
void SvXMLUnitConverter::convertMeasure( OUStringBuffer& rBuffer,
                                         sal_Int32 nMeasure,
                                         MapUnit eSrcUnit,
                                         MapUnit eDstUnit )
{
    if( eSrcUnit == MAP_RELATIVE )
    {
        DBG_ASSERT( eDstUnit == MAP_RELATIVE,
                    "MAP_RELATIVE only maps to MAP_RELATIVE!" );

        rBuffer.append( nMeasure );
        rBuffer.append( sal_Unicode('%' ) );
    }
    else
    {
        SvXMLExportHelper::AddLength( nMeasure, eSrcUnit,
                                      rBuffer, eDstUnit );
    }
}

/** convert string to boolean */
sal_Bool SvXMLUnitConverter::convertBool( sal_Bool& rBool,
                                      const OUString& rString )
{
    rBool = IsXMLToken(rString, XML_TRUE);

    return rBool || IsXMLToken(rString, XML_FALSE);
}

/** convert boolean to string */
void SvXMLUnitConverter::convertBool( OUStringBuffer& rBuffer,
                                      sal_Bool bValue )
{
    rBuffer.append( GetXMLToken( bValue ? XML_TRUE : XML_FALSE ) );
}

/** convert string to percent */
sal_Bool SvXMLUnitConverter::convertPercent( sal_Int32& rPercent,
                                         const OUString& rString )
{
    return convertMeasure( rPercent, rString, MAP_RELATIVE );
}

/** convert percent to string */
void SvXMLUnitConverter::convertPercent( OUStringBuffer& rBuffer,
                                         sal_Int32 nValue )
{
    rBuffer.append( nValue );
    rBuffer.append( sal_Unicode('%' ) );
}

/** convert string to pixel measure */
sal_Bool SvXMLUnitConverter::convertMeasurePx( sal_Int32& rPixel,
                                         const OUString& rString )
{
    return convertMeasure( rPixel, rString, MAP_PIXEL );
}

/** convert pixel measure to string */
void SvXMLUnitConverter::convertMeasurePx( OUStringBuffer& rBuffer,
                                         sal_Int32 nValue )
{
    rBuffer.append( nValue );
    rBuffer.append( sal_Unicode('p' ) );
    rBuffer.append( sal_Unicode('x' ) );
}

/** convert string to enum using given enum map, if the enum is
    not found in the map, this method will return false
*/
sal_Bool SvXMLUnitConverter::convertEnum( sal_uInt16& rEnum,
                                      const OUString& rValue,
                                      const SvXMLEnumStringMapEntry *pMap )
{
    while( pMap->pName )
    {
        if( rValue.equalsAsciiL( pMap->pName, pMap->nNameLength ) )
        {
            rEnum = pMap->nValue;
            return sal_True;
        }
        ++pMap;
    }

    return sal_False;
}

/** convert string to enum using given token map, if the enum is
    not found in the map, this method will return false */
sal_Bool SvXMLUnitConverter::convertEnum(
    sal_uInt16& rEnum,
    const OUString& rValue,
    const SvXMLEnumMapEntry *pMap )
{
    while( pMap->eToken != XML_TOKEN_INVALID )
    {
        if( IsXMLToken( rValue, pMap->eToken ) )
        {
            rEnum = pMap->nValue;
            return sal_True;
        }
        ++pMap;
    }
    return sal_False;
}

/** convert enum to string using given enum map with optional
    default string. If the enum is not found in the map,
    this method will either use the given default or return
    false if not default is set
*/
sal_Bool SvXMLUnitConverter::convertEnum( OUStringBuffer& rBuffer,
                                      sal_uInt16 nValue,
                                      const SvXMLEnumStringMapEntry *pMap,
                                      sal_Char * pDefault /* = NULL */ )
{
    const sal_Char *pStr = pDefault;

    while( pMap->pName )
    {
        if( pMap->nValue == nValue )
        {
            pStr = pMap->pName;
            break;
        }
        ++pMap;
    }

    if( NULL == pStr )
        pStr = pDefault;

    if( NULL != pStr )
        rBuffer.appendAscii( pStr );

    return NULL != pStr;
}

/** convert enum to string using given token map with an optional
    default token. If the enum is not found in the map,
    this method will either use the given default or return
    false if no default is set */
sal_Bool SvXMLUnitConverter::convertEnum(
    OUStringBuffer& rBuffer,
    unsigned int nValue,
    const SvXMLEnumMapEntry *pMap,
    enum XMLTokenEnum eDefault)
{
    enum XMLTokenEnum eTok = eDefault;

    while( pMap->eToken != XML_TOKEN_INVALID )
    {
        if( pMap->nValue == nValue )
        {
            eTok = pMap->eToken;
            break;
        }
        ++pMap;
    }

    // the map may have contained XML_TOKEN_INVALID
    if( eTok == XML_TOKEN_INVALID )
        eTok = eDefault;

    if( eTok != XML_TOKEN_INVALID )
        rBuffer.append( GetXMLToken(eTok) );

    return (eTok != XML_TOKEN_INVALID);
}

int lcl_gethex( int nChar )
{
    if( nChar >= '0' && nChar <= '9' )
        return nChar - '0';
    else if( nChar >= 'a' && nChar <= 'f' )
        return nChar - 'a' + 10;
    else if( nChar >= 'A' && nChar <= 'F' )
        return nChar - 'A' + 10;
    else
        return 0;
}

/** convert string to color */
sal_Bool SvXMLUnitConverter::convertColor( Color& rColor,
                                       const OUString& rValue )
{
    if( rValue.getLength() != 7 || rValue[0] != '#' )
        return sal_False;

    rColor.SetRed(
        sal::static_int_cast< sal_uInt8 >(
            lcl_gethex( rValue[1] ) * 16 + lcl_gethex( rValue[2] ) ) );

    rColor.SetGreen(
        sal::static_int_cast< sal_uInt8 >(
            lcl_gethex( rValue[3] ) * 16 + lcl_gethex( rValue[4] ) ) );

    rColor.SetBlue(
        sal::static_int_cast< sal_uInt8 >(
            lcl_gethex( rValue[5] ) * 16 + lcl_gethex( rValue[6] ) ) );

    return sal_True;
}

static sal_Char aHexTab[] = "0123456789abcdef";

/** convert color to string */
void SvXMLUnitConverter::convertColor( OUStringBuffer& rBuffer,
                                       const Color& rCol )
{
    rBuffer.append( sal_Unicode( '#' ) );

    sal_uInt8 nCol = rCol.GetRed();
    rBuffer.append( sal_Unicode( aHexTab[ nCol >> 4 ] ) );
    rBuffer.append( sal_Unicode( aHexTab[ nCol & 0xf ] ) );

    nCol = rCol.GetGreen();
    rBuffer.append( sal_Unicode( aHexTab[ nCol >> 4 ] ) );
    rBuffer.append( sal_Unicode( aHexTab[ nCol & 0xf ] ) );

    nCol = rCol.GetBlue();
    rBuffer.append( sal_Unicode( aHexTab[ nCol >> 4 ] ) );
    rBuffer.append( sal_Unicode( aHexTab[ nCol & 0xf ] ) );
}

/** convert number to string */
void SvXMLUnitConverter::convertNumber( OUStringBuffer& rBuffer,
                                        sal_Int32 nNumber )
{
    rBuffer.append( sal_Int32( nNumber ) );
}

/** convert string to number with optional min and max values */
sal_Bool SvXMLUnitConverter::convertNumber( sal_Int32& rValue,
                                        const OUString& rString,
                                        sal_Int32 nMin, sal_Int32 nMax )
{
    rValue = 0;
    sal_Int64 nNumber = 0;
    sal_Bool bRet = convertNumber64(nNumber,rString,nMin,nMax);
    if ( bRet )
        rValue = static_cast<sal_Int32>(nNumber);
    return bRet;
}

/** convert 64-bit number to string */
void SvXMLUnitConverter::convertNumber64( OUStringBuffer& rBuffer,
                                        sal_Int64 nNumber )
{
    rBuffer.append( nNumber );
}

/** convert string to 64-bit number with optional min and max values */
sal_Bool SvXMLUnitConverter::convertNumber64( sal_Int64& rValue,
                                        const OUString& rString,
                                        sal_Int64 nMin, sal_Int64 nMax )
{
    sal_Bool bNeg = sal_False;
    rValue = 0;

    sal_Int32 nPos = 0;
    const sal_Int32 nLen = rString.getLength();

    // skip white space
    while( (nPos < nLen) && (rString[nPos] <= sal_Unicode(' ')) )
        ++nPos;

    if( nPos < nLen && sal_Unicode('-') == rString[nPos] )
    {
        bNeg = sal_True;
        ++nPos;
    }

    // get number
    while( nPos < nLen &&
           sal_Unicode('0') <= rString[nPos] &&
           sal_Unicode('9') >= rString[nPos] )
    {
        // TODO: check overflow!
        rValue *= 10;
        rValue += (rString[nPos] - sal_Unicode('0'));
        ++nPos;
    }

    if( bNeg )
        rValue *= -1;

    return ( nPos == nLen && rValue >= nMin && rValue <= nMax );
}

/** convert double number to string (using ::rtl::math) */
void SvXMLUnitConverter::convertDouble(::rtl::OUStringBuffer& rBuffer,
    double fNumber, sal_Bool bWriteUnits) const
{
    SvXMLUnitConverter::convertDouble(rBuffer, fNumber,
        bWriteUnits, meCoreMeasureUnit, meXMLMeasureUnit);
}

/** convert double number to string (using ::rtl::math) */
void SvXMLUnitConverter::convertDouble( ::rtl::OUStringBuffer& rBuffer,
    double fNumber, sal_Bool bWriteUnits, MapUnit eCoreUnit, MapUnit eDstUnit)
{
    if(MAP_RELATIVE == eCoreUnit)
    {
        DBG_ASSERT(eDstUnit == MAP_RELATIVE, "MAP_RELATIVE only maps to MAP_RELATIVE!" );
        ::rtl::math::doubleToUStringBuffer( rBuffer, fNumber, rtl_math_StringFormat_Automatic, rtl_math_DecimalPlaces_Max, '.', sal_True);
        if(bWriteUnits)
            rBuffer.append(sal_Unicode('%'));
    }
    else
    {
        OUStringBuffer sUnit;
        double fFactor = SvXMLExportHelper::GetConversionFactor(sUnit, eCoreUnit, eDstUnit);
        if(fFactor != 1.0)
            fNumber *= fFactor;
        ::rtl::math::doubleToUStringBuffer( rBuffer, fNumber, rtl_math_StringFormat_Automatic, rtl_math_DecimalPlaces_Max, '.', sal_True);
        if(bWriteUnits)
            rBuffer.append(sUnit);
    }
}

/** convert double number to string (using ::rtl::math) */
void SvXMLUnitConverter::convertDouble( ::rtl::OUStringBuffer& rBuffer, double fNumber)
{
    ::rtl::math::doubleToUStringBuffer( rBuffer, fNumber, rtl_math_StringFormat_Automatic, rtl_math_DecimalPlaces_Max, '.', sal_True);
}

/** convert string to double number (using ::rtl::math) */
sal_Bool SvXMLUnitConverter::convertDouble(double& rValue,
    const ::rtl::OUString& rString, sal_Bool bLookForUnits) const
{
    if(bLookForUnits)
    {
        MapUnit eSrcUnit = SvXMLExportHelper::GetUnitFromString(rString, meCoreMeasureUnit);

        return SvXMLUnitConverter::convertDouble(rValue, rString,
            eSrcUnit, meCoreMeasureUnit);
    }
    else
    {
        return SvXMLUnitConverter::convertDouble(rValue, rString);
    }
}

/** convert string to double number (using ::rtl::math) */
sal_Bool SvXMLUnitConverter::convertDouble(double& rValue,
    const ::rtl::OUString& rString, MapUnit eSrcUnit, MapUnit eCoreUnit)
{
    rtl_math_ConversionStatus eStatus;
    rValue = ::rtl::math::stringToDouble( rString, (sal_Unicode)('.'), (sal_Unicode)(','), &eStatus, NULL );

    if(eStatus == rtl_math_ConversionStatus_Ok)
    {
        OUStringBuffer sUnit;
        const double fFactor = SvXMLExportHelper::GetConversionFactor(sUnit, eCoreUnit, eSrcUnit);
        if(fFactor != 1.0 && fFactor != 0.0)
            rValue /= fFactor;
    }

    return ( eStatus == rtl_math_ConversionStatus_Ok );
}

/** convert string to double number (using ::rtl::math) */
sal_Bool SvXMLUnitConverter::convertDouble(double& rValue, const ::rtl::OUString& rString)
{
    rtl_math_ConversionStatus eStatus;
    rValue = ::rtl::math::stringToDouble( rString, (sal_Unicode)('.'), (sal_Unicode)(','), &eStatus, NULL );
    return ( eStatus == rtl_math_ConversionStatus_Ok );
}

/** get the Null Date of the XModel and set it to the UnitConverter */
sal_Bool SvXMLUnitConverter::setNullDate(const com::sun::star::uno::Reference <com::sun::star::frame::XModel>& xModel)
{
    com::sun::star::uno::Reference <com::sun::star::util::XNumberFormatsSupplier> xNumberFormatsSupplier (xModel, com::sun::star::uno::UNO_QUERY);
    if (xNumberFormatsSupplier.is())
    {
        const com::sun::star::uno::Reference <com::sun::star::beans::XPropertySet> xPropertySet = xNumberFormatsSupplier->getNumberFormatSettings();
        return xPropertySet.is() && (xPropertySet->getPropertyValue(rtl::OUString(RTL_CONSTASCII_USTRINGPARAM(XML_NULLDATE))) >>= aNullDate);
    }
    return sal_False;
}

/** convert double to ISO Time String; negative durations allowed */
void SvXMLUnitConverter::convertTime( ::rtl::OUStringBuffer& rBuffer,
                            const double& fTime)
{

    double fValue = fTime;

    // take care of negative durations as specified in:
    // XML Schema, W3C Working Draft 07 April 2000, section 3.2.6.1
    if (fValue < 0.0)
    {
        rBuffer.append(sal_Unicode('-'));
        fValue = - fValue;
    }

    rBuffer.appendAscii(RTL_CONSTASCII_STRINGPARAM( "PT" ));
    fValue *= 24;
    double fHoursValue = ::rtl::math::approxFloor (fValue);
    fValue -= fHoursValue;
    fValue *= 60;
    double fMinsValue = ::rtl::math::approxFloor (fValue);
    fValue -= fMinsValue;
    fValue *= 60;
    double fSecsValue = ::rtl::math::approxFloor (fValue);
    fValue -= fSecsValue;
    double f100SecsValue;
    if (fValue > 0.00001)
        f100SecsValue = ::rtl::math::round( fValue, XML_MAXDIGITSCOUNT_TIME - 5);
    else
        f100SecsValue = 0.0;

    if (f100SecsValue == 1.0)
    {
        f100SecsValue = 0.0;
        fSecsValue += 1.0;
    }
    if (fSecsValue >= 60.0)
    {
        fSecsValue -= 60.0;
        fMinsValue += 1.0;
    }
    if (fMinsValue >= 60.0)
    {
        fMinsValue -= 60.0;
        fHoursValue += 1.0;
    }

    if (fHoursValue < 10)
        rBuffer.append( sal_Unicode('0'));
    rBuffer.append( sal_Int32( fHoursValue));
    rBuffer.append( sal_Unicode('H'));
    if (fMinsValue < 10)
        rBuffer.append( sal_Unicode('0'));
    rBuffer.append( sal_Int32( fMinsValue));
    rBuffer.append( sal_Unicode('M'));
    if (fSecsValue < 10)
        rBuffer.append( sal_Unicode('0'));
    rBuffer.append( sal_Int32( fSecsValue));
    if (f100SecsValue > 0.0)
    {
        ::rtl::OUString a100th( ::rtl::math::doubleToUString( fValue,
                    rtl_math_StringFormat_F, XML_MAXDIGITSCOUNT_TIME - 5, '.',
                    sal_True));
        if ( a100th.getLength() > 2 )
        {
            rBuffer.append( sal_Unicode('.'));
            rBuffer.append( a100th.copy( 2 ) );     // strip 0.
        }
    }
    rBuffer.append( sal_Unicode('S'));
}

/** convert ISO Time String to double; negative durations allowed */
static bool lcl_convertTime( const ::rtl::OUString& rString, sal_Int32& o_rDays, sal_Int32& o_rHours, sal_Int32& o_rMins,
                        sal_Int32& o_rSecs, sal_Bool& o_rIsNegativeTime, double& o_rFractionalSecs )
{
    rtl::OUString aTrimmed = rString.trim().toAsciiUpperCase();
    const sal_Unicode* pStr = aTrimmed.getStr();

    // negative time duration?
    if ( sal_Unicode('-') == (*pStr) )
    {
        o_rIsNegativeTime = sal_True;
        pStr++;
    }

    if ( *(pStr++) != sal_Unicode('P') )            // duration must start with "P"
        return false;

    ::rtl::OUString sDoubleStr;
    sal_Bool bSuccess = true;
    sal_Bool bDone = sal_False;
    sal_Bool bTimePart = sal_False;
    sal_Bool bIsFraction = sal_False;
    sal_Int32 nTemp = 0;

    while ( bSuccess && !bDone )
    {
        sal_Unicode c = *(pStr++);
        if ( !c )                               // end
            bDone = sal_True;
        else if ( sal_Unicode('0') <= c && sal_Unicode('9') >= c )
        {
            if ( nTemp >= SAL_MAX_INT32 / 10 )
                bSuccess = false;
            else
            {
                if ( !bIsFraction )
                {
                    nTemp *= 10;
                    nTemp += (c - sal_Unicode('0'));
                }
                else
                {
                    sDoubleStr += OUString::valueOf(c);
                }
            }
        }
        else if ( bTimePart )
        {
            if ( c == sal_Unicode('H') )
            {
                o_rHours = nTemp;
                nTemp = 0;
            }
            else if ( c == sal_Unicode('M') )
            {
                o_rMins = nTemp;
                nTemp = 0;
            }
            else if ( (c == sal_Unicode(',')) || (c == sal_Unicode('.')) )
            {
                o_rSecs = nTemp;
                nTemp = 0;
                bIsFraction = sal_True;
                sDoubleStr = OUString(RTL_CONSTASCII_USTRINGPARAM("0."));
            }
            else if ( c == sal_Unicode('S') )
            {
                if ( !bIsFraction )
                {
                    o_rSecs = nTemp;
                    nTemp = 0;
                    sDoubleStr = OUString(RTL_CONSTASCII_USTRINGPARAM("0.0"));
                }
            }
            else
                bSuccess = false;                   // invalid character
        }
        else
        {
            if ( c == sal_Unicode('T') )            // "T" starts time part
                bTimePart = sal_True;
            else if ( c == sal_Unicode('D') )
            {
                o_rDays = nTemp;
                nTemp = 0;
            }
            else if ( c == sal_Unicode('Y') || c == sal_Unicode('M') )
            {
                //! how many days is a year or month?

                DBG_ERROR("years or months in duration: not implemented");
                bSuccess = false;
            }
            else
                bSuccess = false;                   // invalid character
        }
    }

    if ( bSuccess )
        o_rFractionalSecs = sDoubleStr.toDouble();
    return bSuccess;
}

sal_Bool SvXMLUnitConverter::convertTime( double& fTime,
                            const ::rtl::OUString& rString)
{
    sal_Int32 nDays  = 0;
    sal_Int32 nHours = 0;
    sal_Int32 nMins  = 0;
    sal_Int32 nSecs  = 0;
    sal_Bool bIsNegativeDuration = sal_False;
    double fFractionalSecs = 0.0;
    if ( lcl_convertTime( rString, nDays, nHours, nMins, nSecs, bIsNegativeDuration, fFractionalSecs ) )
    {
        if ( nDays )
            nHours += nDays * 24;               // add the days to the hours part
        double fTempTime = 0.0;
        double fHour = nHours;
        double fMin = nMins;
        double fSec = nSecs;
        double fSec100 = 0.0;
        fTempTime = fHour / 24;
        fTempTime += fMin / (24 * 60);
        fTempTime += fSec / (24 * 60 * 60);
        fTempTime += fSec100 / (24 * 60 * 60 * 60);
        fTempTime += fFractionalSecs / (24 * 60 * 60);

        // negative duration?
        if ( bIsNegativeDuration )
        {
            fTempTime = -fTempTime;
        }

        fTime = fTempTime;
        return sal_True;
    }
    return sal_False;
}

/** convert util::DateTime to ISO Time String */
void SvXMLUnitConverter::convertTime( ::rtl::OUStringBuffer& rBuffer,
                            const ::com::sun::star::util::DateTime& rDateTime )
{
    double fHour = rDateTime.Hours;
    double fMin = rDateTime.Minutes;
    double fSec = rDateTime.Seconds;
    double fSec100 = rDateTime.HundredthSeconds;
    double fTempTime = fHour / 24;
    fTempTime += fMin / (24 * 60);
    fTempTime += fSec / (24 * 60 * 60);
    fTempTime += fSec100 / (24 * 60 * 60 * 100);
    convertTime( rBuffer, fTempTime );
}

/** convert ISO Time String to util::DateTime */
sal_Bool SvXMLUnitConverter::convertTime( ::com::sun::star::util::DateTime& rDateTime,
                             const ::rtl::OUString& rString )
{
    sal_Int32 nDays = 0, nHours = 0, nMins = 0, nSecs = 0;
    sal_Bool bIsNegativeDuration = sal_False;
    double fFractionalSecs = 0.0;
    if ( lcl_convertTime( rString, nDays, nHours, nMins, nSecs, bIsNegativeDuration, fFractionalSecs ) )
    {
        rDateTime.Year = 0;
        rDateTime.Month = 0;
        rDateTime.Day = 0;
        rDateTime.Hours = static_cast < sal_uInt16 > ( nHours );
        rDateTime.Minutes = static_cast < sal_uInt16 > ( nMins );
        rDateTime.Seconds = static_cast < sal_uInt16 > ( nSecs );
        rDateTime.HundredthSeconds = static_cast < sal_uInt16 > ( fFractionalSecs * 100.0 );

        return sal_True;
    }
    return sal_False;
}

/** convert double to ISO Date Time String */
void SvXMLUnitConverter::convertDateTime( ::rtl::OUStringBuffer& rBuffer,
        const double& fDateTime, 
		const com::sun::star::util::Date& aTempNullDate,
		sal_Bool bAddTimeIf0AM )
{
    double fValue = fDateTime;
    sal_Int32 nValue = static_cast <sal_Int32> (::rtl::math::approxFloor (fValue));
    Date aDate (aTempNullDate.Day, aTempNullDate.Month, aTempNullDate.Year);
    aDate += nValue;
    fValue -= nValue;
    double fCount;
    if (nValue > 0)
         fCount = ::rtl::math::approxFloor (log10((double)nValue)) + 1;
    else if (nValue < 0)
         fCount = ::rtl::math::approxFloor (log10((double)(nValue * -1))) + 1;
    else
        fCount = 0.0;
    sal_Int16 nCount = sal_Int16(fCount);
    sal_Bool bHasTime(sal_False);
    double fHoursValue = 0;
    double fMinsValue = 0;
    double fSecsValue = 0;
    double f100SecsValue = 0;
    if (fValue > 0.0)
    {
        bHasTime = sal_True;
        fValue *= 24;
        fHoursValue = ::rtl::math::approxFloor (fValue);
        fValue -= fHoursValue;
        fValue *= 60;
        fMinsValue = ::rtl::math::approxFloor (fValue);
        fValue -= fMinsValue;
        fValue *= 60;
        fSecsValue = ::rtl::math::approxFloor (fValue);
        fValue -= fSecsValue;
        if (fValue > 0.0)
            f100SecsValue = ::rtl::math::round( fValue, XML_MAXDIGITSCOUNT_TIME - nCount);
        else
            f100SecsValue = 0.0;

        if (f100SecsValue == 1.0)
        {
            f100SecsValue = 0.0;
            fSecsValue += 1.0;
        }
        if (fSecsValue >= 60.0)
        {
            fSecsValue -= 60.0;
            fMinsValue += 1.0;
        }
        if (fMinsValue >= 60.0)
        {
            fMinsValue -= 60.0;
            fHoursValue += 1.0;
        }
        if (fHoursValue >= 24.0)
        {
            fHoursValue -= 24.0;
            aDate += 1;
        }
    }
    rBuffer.append( sal_Int32( aDate.GetYear()));
    rBuffer.append( sal_Unicode('-'));
    sal_uInt16 nTemp = aDate.GetMonth();
    if (nTemp < 10)
        rBuffer.append( sal_Unicode('0'));
    rBuffer.append( sal_Int32( nTemp));
    rBuffer.append( sal_Unicode('-'));
    nTemp = aDate.GetDay();
    if (nTemp < 10)
        rBuffer.append( sal_Unicode('0'));
    rBuffer.append( sal_Int32( nTemp));
    if(bHasTime || bAddTimeIf0AM)
    {
        rBuffer.append( sal_Unicode('T'));
        if (fHoursValue < 10)
            rBuffer.append( sal_Unicode('0'));
        rBuffer.append( sal_Int32( fHoursValue));
        rBuffer.append( sal_Unicode(':'));
        if (fMinsValue < 10)
            rBuffer.append( sal_Unicode('0'));
        rBuffer.append( sal_Int32( fMinsValue));
        rBuffer.append( sal_Unicode(':'));
        if (fSecsValue < 10)
            rBuffer.append( sal_Unicode('0'));
        rBuffer.append( sal_Int32( fSecsValue));
        if (f100SecsValue > 0.0)
        {
            ::rtl::OUString a100th( ::rtl::math::doubleToUString( fValue,
                        rtl_math_StringFormat_F,
                        XML_MAXDIGITSCOUNT_TIME - nCount, '.', sal_True));
            if ( a100th.getLength() > 2 )
            {
                rBuffer.append( sal_Unicode('.'));
                rBuffer.append( a100th.copy( 2 ) );     // strip 0.
            }
        }
    }
}

/** convert ISO Date Time String to double */
sal_Bool SvXMLUnitConverter::convertDateTime( double& fDateTime,
                            const ::rtl::OUString& rString, const com::sun::star::util::Date& aTempNullDate)
{
    com::sun::star::util::DateTime aDateTime;
    sal_Bool bSuccess = convertDateTime(aDateTime,rString);

    if (bSuccess)
    {
        double fTempDateTime = 0.0;
        const Date aTmpNullDate(aTempNullDate.Day, aTempNullDate.Month, aTempNullDate.Year);
        const Date aTempDate((sal_uInt16)aDateTime.Day, (sal_uInt16)aDateTime.Month, (sal_uInt16)aDateTime.Year);
        const sal_Int32 nTage = aTempDate - aTmpNullDate;
        fTempDateTime = nTage;
        double Hour = aDateTime.Hours;
        double Min = aDateTime.Minutes;
        double Sec = aDateTime.Seconds;
        double Sec100 = aDateTime.HundredthSeconds;
        fTempDateTime += Hour / 24;
        fTempDateTime += Min / (24 * 60);
        fTempDateTime += Sec / (24 * 60 * 60);
        fTempDateTime += Sec100 / (24 * 60 * 60 * 100);
        fDateTime = fTempDateTime;
    }
    return bSuccess;
}

/** convert util::DateTime to ISO Date String */
void SvXMLUnitConverter::convertDateTime( 
				::rtl::OUStringBuffer& rBuffer,
                const com::sun::star::util::DateTime& rDateTime,
				sal_Bool bAddTimeIf0AM )
{
    String aString( String::CreateFromInt32( rDateTime.Year ) );
    aString += '-';
    if( rDateTime.Month < 10 )
        aString += '0';
    aString += String::CreateFromInt32( rDateTime.Month );
    aString += '-';
    if( rDateTime.Day < 10 )
        aString += '0';
    aString += String::CreateFromInt32( rDateTime.Day );

    if( rDateTime.Seconds != 0 ||
        rDateTime.Minutes != 0 ||
        rDateTime.Hours   != 0 ||
		bAddTimeIf0AM )
    {
        aString += 'T';
        if( rDateTime.Hours < 10 )
            aString += '0';
        aString += String::CreateFromInt32( rDateTime.Hours );
        aString += ':';
        if( rDateTime.Minutes < 10 )
            aString += '0';
        aString += String::CreateFromInt32( rDateTime.Minutes );
        aString += ':';
        if( rDateTime.Seconds < 10 )
            aString += '0';
        aString += String::CreateFromInt32( rDateTime.Seconds );
		if ( rDateTime.HundredthSeconds > 0)
		{
	        aString += '.';
			if (rDateTime.HundredthSeconds < 10)
				aString += '0';
			aString += String::CreateFromInt32( rDateTime.HundredthSeconds );
		}
    }

    rBuffer.append( aString );
}

/** convert ISO Date String to util::DateTime */
sal_Bool SvXMLUnitConverter::convertDateTime( com::sun::star::util::DateTime& rDateTime,
                                     const ::rtl::OUString& rString )
{
    sal_Bool bSuccess = sal_True;

    rtl::OUString aDateStr, aTimeStr, sDoubleStr;
    sal_Int32 nPos = rString.indexOf( (sal_Unicode) 'T' );
    sal_Int32 nPos2 = rString.indexOf( (sal_Unicode) ',' );
    if (nPos2 < 0)
        nPos2 = rString.indexOf( (sal_Unicode) '.' );
    if ( nPos >= 0 )
    {
        aDateStr = rString.copy( 0, nPos );
        if ( nPos2 >= 0 )
        {
            aTimeStr = rString.copy( nPos + 1, nPos2 - nPos - 1 );
            sDoubleStr = OUString(RTL_CONSTASCII_USTRINGPARAM("0."));
            sDoubleStr += rString.copy( nPos2 + 1 );
        }
        else
        {
            aTimeStr = rString.copy(nPos + 1);
            sDoubleStr = OUString(RTL_CONSTASCII_USTRINGPARAM("0.0"));
        }
    }
    else
        aDateStr = rString;         // no separator: only date part

    sal_Int32 nYear  = 1899;
    sal_Int32 nMonth = 12;
    sal_Int32 nDay   = 30;
    sal_Int32 nHour  = 0;
    sal_Int32 nMin   = 0;
    sal_Int32 nSec   = 0;

    const sal_Unicode* pStr = aDateStr.getStr();
    sal_Int32 nDateTokens = 1;
    while ( *pStr )
    {
        if ( *pStr == '-' )
            nDateTokens++;
        pStr++;
    }
    if ( nDateTokens > 3 || aDateStr.getLength() == 0 )
        bSuccess = sal_False;
    else
    {
        sal_Int32 n = 0;
        if ( !convertNumber( nYear, aDateStr.getToken( 0, '-', n ), 0, 9999 ) )
            bSuccess = sal_False;
        if ( nDateTokens >= 2 )
            if ( !convertNumber( nMonth, aDateStr.getToken( 0, '-', n ), 0, 12 ) )
                bSuccess = sal_False;
        if ( nDateTokens >= 3 )
            if ( !convertNumber( nDay, aDateStr.getToken( 0, '-', n ), 0, 31 ) )
                bSuccess = sal_False;
    }

    if ( aTimeStr.getLength() > 0 )           // time is optional
    {
        pStr = aTimeStr.getStr();
        sal_Int32 nTimeTokens = 1;
        while ( *pStr )
        {
            if ( *pStr == ':' )
                nTimeTokens++;
            pStr++;
        }
        if ( nTimeTokens > 3 )
            bSuccess = sal_False;
        else
        {
            sal_Int32 n = 0;
            if ( !convertNumber( nHour, aTimeStr.getToken( 0, ':', n ), 0, 23 ) )
                bSuccess = sal_False;
            if ( nTimeTokens >= 2 )
                if ( !convertNumber( nMin, aTimeStr.getToken( 0, ':', n ), 0, 59 ) )
                    bSuccess = sal_False;
            if ( nTimeTokens >= 3 )
                if ( !convertNumber( nSec, aTimeStr.getToken( 0, ':', n ), 0, 59 ) )
                    bSuccess = sal_False;
        }
    }

    if (bSuccess)
    {
        rDateTime.Year = (sal_uInt16)nYear;
        rDateTime.Month = (sal_uInt16)nMonth;
        rDateTime.Day = (sal_uInt16)nDay;
        rDateTime.Hours = (sal_uInt16)nHour;
        rDateTime.Minutes = (sal_uInt16)nMin;
        rDateTime.Seconds = (sal_uInt16)nSec;
        rDateTime.HundredthSeconds = (sal_uInt16)(sDoubleStr.toDouble() * 100);
    }
    return bSuccess;
}

/** gets the position of the first comma after npos in the string
    rStr. Commas inside '"' pairs are not matched */
sal_Int32 SvXMLUnitConverter::indexOfComma( const OUString& rStr,
                                            sal_Int32 nPos )
{
    sal_Unicode cQuote = 0;
    sal_Int32 nLen = rStr.getLength();
    for( ; nPos < nLen; nPos++ )
    {
        sal_Unicode c = rStr[nPos];
        switch( c )
        {
        case sal_Unicode('\''):
            if( 0 == cQuote )
                cQuote = c;
            else if( '\'' == cQuote )
                cQuote = 0;
            break;

        case sal_Unicode('"'):
            if( 0 == cQuote )
                cQuote = c;
            else if( '\"' == cQuote )
                cQuote = 0;
            break;

        case sal_Unicode(','):
            if( 0 == cQuote )
                return nPos;
            break;
        }
    }

    return -1;
}

// ---

SvXMLTokenEnumerator::SvXMLTokenEnumerator( const OUString& rString, sal_Unicode cSeperator /* = sal_Unicode(' ') */ )
: maTokenString( rString ), mnNextTokenPos(0), mcSeperator( cSeperator )
{
}

sal_Bool SvXMLTokenEnumerator::getNextToken( OUString& rToken )
{
    if( -1 == mnNextTokenPos )
        return sal_False;

    int nTokenEndPos = maTokenString.indexOf( mcSeperator, mnNextTokenPos );
    if( nTokenEndPos != -1 )
    {
        rToken = maTokenString.copy( mnNextTokenPos,
                                     nTokenEndPos - mnNextTokenPos );
        mnNextTokenPos = nTokenEndPos + 1;

        // if the mnNextTokenPos is at the end of the string, we have
        // to deliver an empty token
        if( mnNextTokenPos > maTokenString.getLength() )
            mnNextTokenPos = -1;
    }
    else
    {
        rToken = maTokenString.copy( mnNextTokenPos );
        mnNextTokenPos = -1;
    }

    return sal_True;
}

// ---
bool lcl_getPositions(const OUString& _sValue,OUString& _rContentX,OUString& _rContentY,OUString& _rContentZ)
{
    if(!_sValue.getLength() || _sValue[0] != '(')
        return false;

    sal_Int32 nPos(1L);
    sal_Int32 nFound = _sValue.indexOf(sal_Unicode(' '), nPos);

    if(nFound == -1 || nFound <= nPos)
        return false;

    _rContentX = _sValue.copy(nPos, nFound - nPos);

    nPos = nFound + 1;
    nFound = _sValue.indexOf(sal_Unicode(' '), nPos);

    if(nFound == -1 || nFound <= nPos)
        return false;

    _rContentY = _sValue.copy(nPos, nFound - nPos);

    nPos = nFound + 1;
    nFound = _sValue.indexOf(sal_Unicode(')'), nPos);

    if(nFound == -1 || nFound <= nPos)
        return false;

    _rContentZ = _sValue.copy(nPos, nFound - nPos);
    return true;

}
/** convert string to ::basegfx::B3DVector */
sal_Bool SvXMLUnitConverter::convertB3DVector( ::basegfx::B3DVector& rVector, const OUString& rValue )
{
    OUString aContentX,aContentY,aContentZ;
    if ( !lcl_getPositions(rValue,aContentX,aContentY,aContentZ) )
        return sal_False;

    rtl_math_ConversionStatus eStatus;

    rVector.setX(::rtl::math::stringToDouble(aContentX, sal_Unicode('.'),
            sal_Unicode(','), &eStatus, NULL));

    if( eStatus != rtl_math_ConversionStatus_Ok )
        return sal_False;

    rVector.setY(::rtl::math::stringToDouble(aContentY, sal_Unicode('.'),
            sal_Unicode(','), &eStatus, NULL));

    if( eStatus != rtl_math_ConversionStatus_Ok )
        return sal_False;

    rVector.setZ(::rtl::math::stringToDouble(aContentZ, sal_Unicode('.'),
            sal_Unicode(','), &eStatus, NULL));


    return ( eStatus == rtl_math_ConversionStatus_Ok );
}

/** convert ::basegfx::B3DVector to string */
void SvXMLUnitConverter::convertB3DVector( OUStringBuffer &rBuffer, const ::basegfx::B3DVector& rVector )
{
    rBuffer.append(sal_Unicode('('));
    convertDouble(rBuffer, rVector.getX());
    rBuffer.append(sal_Unicode(' '));
    convertDouble(rBuffer, rVector.getY());
    rBuffer.append(sal_Unicode(' '));
    convertDouble(rBuffer, rVector.getZ());
    rBuffer.append(sal_Unicode(')'));
}

/** convert string to Position3D */
sal_Bool SvXMLUnitConverter::convertPosition3D( drawing::Position3D& rPosition,
    const OUString& rValue )
{
    OUString aContentX,aContentY,aContentZ;
    if ( !lcl_getPositions(rValue,aContentX,aContentY,aContentZ) )
        return sal_False;

	if ( !convertDouble( rPosition.PositionX, aContentX, sal_True ) )
		return sal_False;
	if ( !convertDouble( rPosition.PositionY, aContentY, sal_True ) )
		return sal_False;
	return convertDouble( rPosition.PositionZ, aContentZ, sal_True );
}

/** convert Position3D to string */
void SvXMLUnitConverter::convertPosition3D( OUStringBuffer &rBuffer,
										   const drawing::Position3D& rPosition )
{
    rBuffer.append( sal_Unicode('(') );
    convertDouble( rBuffer, rPosition.PositionX, sal_True );
    rBuffer.append( sal_Unicode(' ') );
    convertDouble( rBuffer, rPosition.PositionY, sal_True );
    rBuffer.append( sal_Unicode(' ') );
    convertDouble( rBuffer, rPosition.PositionZ, sal_True );
    rBuffer.append( sal_Unicode(')') );
}

const
  sal_Char aBase64EncodeTable[] =
    { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };

const
  sal_uInt8 aBase64DecodeTable[]  =
    {											 62,255,255,255, 63, // 43-47
//                                                +               /

     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255, // 48-63
//    0   1   2   3   4   5   6   7   8   9               =

    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 64-79
//        A   B   C   D   E   F   G   H   I   J   K   L   M   N   O

     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255, // 80-95
//    P   Q   R   S   T   U   V   W   X   Y   Z

      0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, // 96-111
//        a   b   c   d   e   f   g   h   i   j   k   l   m   n   o

     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 }; // 112-123
//    p   q   r   s   t   u   v   w   x   y   z



void ThreeByteToFourByte (const sal_Int8* pBuffer, const sal_Int32 nStart, const sal_Int32 nFullLen, rtl::OUStringBuffer& sBuffer)
{
    sal_Int32 nLen(nFullLen - nStart);
    if (nLen > 3)
        nLen = 3;
    if (nLen == 0)
    {
        sBuffer.setLength(0);
        return;
    }

    sal_Int32 nBinaer;
    switch (nLen)
    {
        case 1:
        {
            nBinaer = ((sal_uInt8)pBuffer[nStart + 0]) << 16;
        }
        break;
        case 2:
        {
            nBinaer = (((sal_uInt8)pBuffer[nStart + 0]) << 16) +
                    (((sal_uInt8)pBuffer[nStart + 1]) <<  8);
        }
        break;
        default:
        {
            nBinaer = (((sal_uInt8)pBuffer[nStart + 0]) << 16) +
                    (((sal_uInt8)pBuffer[nStart + 1]) <<  8) +
                    ((sal_uInt8)pBuffer[nStart + 2]);
        }
        break;
    }

    sBuffer.appendAscii("====");

    sal_uInt8 nIndex (static_cast<sal_uInt8>((nBinaer & 0xFC0000) >> 18));
    sBuffer.setCharAt(0, aBase64EncodeTable [nIndex]);

    nIndex = static_cast<sal_uInt8>((nBinaer & 0x3F000) >> 12);
    sBuffer.setCharAt(1, aBase64EncodeTable [nIndex]);
    if (nLen == 1)
        return;

    nIndex = static_cast<sal_uInt8>((nBinaer & 0xFC0) >> 6);
    sBuffer.setCharAt(2, aBase64EncodeTable [nIndex]);
    if (nLen == 2)
        return;

    nIndex = static_cast<sal_uInt8>((nBinaer & 0x3F));
    sBuffer.setCharAt(3, aBase64EncodeTable [nIndex]);
}

void SvXMLUnitConverter::encodeBase64(rtl::OUStringBuffer& aStrBuffer, const uno::Sequence<sal_Int8>& aPass)
{
	sal_Int32 i(0);
	sal_Int32 nBufferLength(aPass.getLength());
	const sal_Int8* pBuffer = aPass.getConstArray();
	while (i < nBufferLength)
	{
		rtl::OUStringBuffer sBuffer;
		ThreeByteToFourByte (pBuffer, i, nBufferLength, sBuffer);
		aStrBuffer.append(sBuffer);
		i += 3;
	}
}

void SvXMLUnitConverter::decodeBase64(uno::Sequence<sal_Int8>& aBuffer, const rtl::OUString& sBuffer)
{
	sal_Int32 nCharsDecoded = decodeBase64SomeChars( aBuffer, sBuffer );
	OSL_ENSURE( nCharsDecoded == sBuffer.getLength(),
				"some bytes left in base64 decoding!" );
	(void)nCharsDecoded;
}

sal_Int32 SvXMLUnitConverter::decodeBase64SomeChars(
		uno::Sequence<sal_Int8>& rOutBuffer,
		const rtl::OUString& rInBuffer)
{
	sal_Int32 nInBufferLen = rInBuffer.getLength();
	sal_Int32 nMinOutBufferLen = (nInBufferLen / 4) * 3;
	if( rOutBuffer.getLength() < nMinOutBufferLen )
		rOutBuffer.realloc( nMinOutBufferLen );

	const sal_Unicode *pInBuffer = rInBuffer.getStr();
	sal_Int8 *pOutBuffer = rOutBuffer.getArray();
	sal_Int8 *pOutBufferStart = pOutBuffer;
	sal_Int32 nCharsDecoded = 0;

	sal_uInt8 aDecodeBuffer[4];
	sal_Int32 nBytesToDecode = 0;
	sal_Int32 nBytesGotFromDecoding = 3;
	sal_Int32 nInBufferPos= 0;
	while( nInBufferPos < nInBufferLen )
	{
		sal_Unicode cChar = *pInBuffer;
		if( cChar >= '+' && cChar <= 'z' )
		{
			sal_uInt8 nByte = aBase64DecodeTable[cChar-'+'];
			if( nByte != 255 )
			{
				// We have found a valid character!
				aDecodeBuffer[nBytesToDecode++] = nByte;

				// One '=' character at the end means 2 out bytes
				// Two '=' characters at the end mean 1 out bytes
				if( '=' == cChar && nBytesToDecode > 2 )
					nBytesGotFromDecoding--;
				if( 4 == nBytesToDecode )
				{
					// Four characters found, so we may convert now!
					sal_uInt32 nOut = (aDecodeBuffer[0] << 18) +
									  (aDecodeBuffer[1] << 12) +
									  (aDecodeBuffer[2] << 6) +
									   aDecodeBuffer[3];

					*pOutBuffer++  = (sal_Int8)((nOut & 0xff0000) >> 16);
					if( nBytesGotFromDecoding > 1 )
						*pOutBuffer++  = (sal_Int8)((nOut & 0xff00) >> 8);
					if( nBytesGotFromDecoding > 2 )
						*pOutBuffer++  = (sal_Int8)(nOut & 0xff);
					nCharsDecoded = nInBufferPos + 1;
					nBytesToDecode = 0;
					nBytesGotFromDecoding = 3;
				}
			}
			else
			{
				nCharsDecoded++;
			}
		}
		else
		{
			nCharsDecoded++;
		}

		nInBufferPos++;
		pInBuffer++;
	}
	if( (pOutBuffer - pOutBufferStart) != rOutBuffer.getLength() )
		rOutBuffer.realloc( pOutBuffer - pOutBufferStart );

	return nCharsDecoded;
}

sal_Bool SvXMLUnitConverter::convertNumFormat(
        sal_Int16& rType,
        const OUString& rNumFmt,
        const OUString& rNumLetterSync,
        sal_Bool bNumberNone ) const
{
    sal_Bool bRet = sal_True;
    sal_Bool bExt = sal_False;

    sal_Int32 nLen = rNumFmt.getLength();
    if( 0 == nLen )
    {
        if( bNumberNone )
            rType = NumberingType::NUMBER_NONE;
        else
            bRet = sal_False;
    }
    else if( 1 == nLen )
    {
        switch( rNumFmt[0] )
        {
        case sal_Unicode('1'):  rType = NumberingType::ARABIC;          break;
        case sal_Unicode('a'):  rType = NumberingType::CHARS_LOWER_LETTER;  break;
        case sal_Unicode('A'):  rType = NumberingType::CHARS_UPPER_LETTER;  break;
        case sal_Unicode('i'):  rType = NumberingType::ROMAN_LOWER; break;
        case sal_Unicode('I'):  rType = NumberingType::ROMAN_UPPER; break;
        default:                bExt = sal_True; break;
        }
        if( !bExt && IsXMLToken( rNumLetterSync, XML_TRUE ) )
        {
            switch( rType )
            {
            case NumberingType::CHARS_LOWER_LETTER:
                rType = NumberingType::CHARS_LOWER_LETTER_N;
                break;
            case NumberingType::CHARS_UPPER_LETTER:
                rType = NumberingType::CHARS_UPPER_LETTER_N;
                break;
            }
        }
    }
    else
    {
        bExt = sal_True;
    }
    if( bExt )
    {
        Reference < XNumberingTypeInfo > xInfo = getNumTypeInfo();
        if( xInfo.is() && xInfo->hasNumberingType( rNumFmt ) )
        {
            rType = xInfo->getNumberingType( rNumFmt );
        }
        else
        {
            rType = NumberingType::ARABIC;
        }
    }

    return bRet;
}

void SvXMLUnitConverter::convertNumFormat( OUStringBuffer& rBuffer,
                           sal_Int16 nType ) const
{
    enum XMLTokenEnum eFormat = XML_TOKEN_INVALID;
    sal_Bool bExt = sal_False;
    switch( nType )
    {
    case NumberingType::CHARS_UPPER_LETTER:     eFormat = XML_A_UPCASE; break;
    case NumberingType::CHARS_LOWER_LETTER:     eFormat = XML_A; break;
    case NumberingType::ROMAN_UPPER:            eFormat = XML_I_UPCASE; break;
    case NumberingType::ROMAN_LOWER:            eFormat = XML_I; break;
    case NumberingType::ARABIC:                 eFormat = XML_1; break;
    case NumberingType::CHARS_UPPER_LETTER_N:   eFormat = XML_A_UPCASE; break;
    case NumberingType::CHARS_LOWER_LETTER_N:   eFormat = XML_A; break;
    case NumberingType::NUMBER_NONE:            eFormat = XML__EMPTY; break;

    case NumberingType::CHAR_SPECIAL:
    case NumberingType::PAGE_DESCRIPTOR:
    case NumberingType::BITMAP:
        DBG_ASSERT( eFormat != XML_TOKEN_INVALID, "invalid number format" );
        break;
    default:
        bExt = sal_True;
        break;
    }

    if( eFormat != XML_TOKEN_INVALID )
    {
        rBuffer.append( GetXMLToken(eFormat) );
    }
    else
    {
        Reference < XNumberingTypeInfo > xInfo = getNumTypeInfo();
        if( xInfo.is() )
            rBuffer.append( xInfo->getNumberingIdentifier( nType ) );
    }
}

void SvXMLUnitConverter::convertNumLetterSync( OUStringBuffer& rBuffer,
                               sal_Int16 nType ) const
{
    enum XMLTokenEnum eSync = XML_TOKEN_INVALID;
    switch( nType )
    {
    case NumberingType::CHARS_UPPER_LETTER:
    case NumberingType::CHARS_LOWER_LETTER:
    case NumberingType::ROMAN_UPPER:
    case NumberingType::ROMAN_LOWER:
    case NumberingType::ARABIC:
    case NumberingType::NUMBER_NONE:
        // default
        // eSync = XML_FALSE;
        break;

    case NumberingType::CHARS_UPPER_LETTER_N:
    case NumberingType::CHARS_LOWER_LETTER_N:
        eSync = XML_TRUE;
        break;

    case NumberingType::CHAR_SPECIAL:
    case NumberingType::PAGE_DESCRIPTOR:
    case NumberingType::BITMAP:
        DBG_ASSERT( eSync != XML_TOKEN_INVALID, "invalid number format" );
        break;
    }
    if( eSync != XML_TOKEN_INVALID )
        rBuffer.append( GetXMLToken(eSync) );
}

void SvXMLUnitConverter::convertPropertySet(uno::Sequence<beans::PropertyValue>& rProps,
                    const uno::Reference<beans::XPropertySet>& aProperties)
{
    uno::Reference< beans::XPropertySetInfo > xPropertySetInfo = aProperties->getPropertySetInfo();
    if (xPropertySetInfo.is())
    {
        uno::Sequence< beans::Property > aProps = xPropertySetInfo->getProperties();
        const sal_Int32 nCount(aProps.getLength());
        if (nCount)
        {
            rProps.realloc(nCount);
            beans::PropertyValue* pProps = rProps.getArray();
            for (sal_Int32 i = 0; i < nCount; i++, ++pProps)
            {
                pProps->Name = aProps[i].Name;
                pProps->Value = aProperties->getPropertyValue(aProps[i].Name);
            }
        }
    }
}

void SvXMLUnitConverter::convertPropertySet(uno::Reference<beans::XPropertySet>& rProperties,
                    const uno::Sequence<beans::PropertyValue>& aProps)
{
    sal_Int32 nCount(aProps.getLength());
    if (nCount)
    {
        uno::Reference< beans::XPropertySetInfo > xPropertySetInfo = rProperties->getPropertySetInfo();
        if (xPropertySetInfo.is())
        {
            for (sal_Int32 i = 0; i < nCount; i++)
            {
                if (xPropertySetInfo->hasPropertyByName(aProps[i].Name))
                    rProperties->setPropertyValue(aProps[i].Name, aProps[i].Value);
            }
        }
    }
}

void SvXMLUnitConverter::clearUndefinedChars(rtl::OUString& rTarget, const rtl::OUString& rSource)
{
	sal_uInt32 nLength(rSource.getLength());
	rtl::OUStringBuffer sBuffer(nLength);
	for (sal_uInt32 i = 0; i < nLength; i++)
	{
		sal_Unicode cChar = rSource[i];
		if (!(cChar < 0x0020) ||
			(cChar == 0x0009) ||		// TAB
			(cChar == 0x000A) ||		// LF
			(cChar == 0x000D))			// legal character
			sBuffer.append(cChar);
	}
	rTarget = sBuffer.makeStringAndClear();
}

OUString SvXMLUnitConverter::encodeStyleName( 
		const OUString& rName, 
	    sal_Bool *pEncoded ) const
{
	if( pEncoded )
		*pEncoded = sal_False;

	sal_Int32 nLen = rName.getLength();
	OUStringBuffer aBuffer( nLen );

	for( sal_Int32 i = 0; i < nLen; i++ )
	{
		sal_Unicode c = rName[i];
		sal_Bool bValidChar = sal_False;
		if( c < 0x00ffU )
		{
			bValidChar = 
				(c >= 0x0041 && c <= 0x005a) ||
				(c >= 0x0061 && c <= 0x007a) ||
				(c >= 0x00c0 && c <= 0x00d6) ||
				(c >= 0x00d8 && c <= 0x00f6) ||
				(c >= 0x00f8 && c <= 0x00ff) ||
				( i > 0 && ( (c >= 0x0030 && c <= 0x0039) ||
							 c == 0x00b7 || c == '-' || c == '.') );
		}
		else
		{
			if( (c >= 0xf900U && c <= 0xfffeU) ||
			 	(c >= 0x20ddU && c <= 0x20e0U))
			{
				bValidChar = sal_False;
			}
			else if( (c >= 0x02bbU && c <= 0x02c1U) || c == 0x0559 ||
					 c == 0x06e5 || c == 0x06e6 )
			{
				bValidChar = sal_True;
			}
			else if( c == 0x0387 )
			{
				bValidChar = i > 0;
			}
			else
			{
				if( !xCharClass.is() )
				{
					if( mxServiceFactory.is() )
					{
						try
						{
							const_cast < SvXMLUnitConverter * >(this)
								->xCharClass = 
									Reference < XCharacterClassification >(
								mxServiceFactory->createInstance(
									OUString::createFromAscii(
						"com.sun.star.i18n.CharacterClassification_Unicode") ),
								UNO_QUERY );

							OSL_ENSURE( xCharClass.is(),
					"can't instantiate character clossification component" );
						}
						catch( com::sun::star::uno::Exception& )
						{
						}
					}
				}
				if( xCharClass.is() )
				{
					sal_Int16 nType = xCharClass->getType( rName, i );

					switch( nType )
					{
					case UnicodeType::UPPERCASE_LETTER:		// Lu
					case UnicodeType::LOWERCASE_LETTER:		// Ll
					case UnicodeType::TITLECASE_LETTER:		// Lt
					case UnicodeType::OTHER_LETTER:			// Lo
					case UnicodeType::LETTER_NUMBER: 		// Nl
						bValidChar = sal_True;
						break;
					case UnicodeType::NON_SPACING_MARK:		// Ms
					case UnicodeType::ENCLOSING_MARK:		// Me
					case UnicodeType::COMBINING_SPACING_MARK:	//Mc
					case UnicodeType::MODIFIER_LETTER:		// Lm
					case UnicodeType::DECIMAL_DIGIT_NUMBER:	// Nd
						bValidChar = i > 0;
						break;
					}
				}
			}
		}
		if( bValidChar )
		{
			aBuffer.append( c );
		}
		else
		{
			aBuffer.append( static_cast< sal_Unicode >( '_' ) );
			if( c > 0x0fff )
				aBuffer.append( static_cast< sal_Unicode >( 
							aHexTab[ (c >> 12) & 0x0f ]  ) );
			if( c > 0x00ff )
				aBuffer.append( static_cast< sal_Unicode >( 
						aHexTab[ (c >> 8) & 0x0f ] ) );
			if( c > 0x000f )
				aBuffer.append( static_cast< sal_Unicode >( 
						aHexTab[ (c >> 4) & 0x0f ] ) );
			aBuffer.append( static_cast< sal_Unicode >( 
						aHexTab[ c & 0x0f ] ) );
			aBuffer.append( static_cast< sal_Unicode >( '_' ) );
			if( pEncoded )
				*pEncoded = sal_True;
		}
	}

	// check for length
	if( aBuffer.getLength() > ((1<<15)-1) )
	{
		aBuffer = rName;
		if( pEncoded )
			*pEncoded = sal_False;
	}


	return aBuffer.makeStringAndClear();
}

// static
rtl::OUString SvXMLUnitConverter::convertTimeDuration( const Time& rTime, sal_Int32 nSecondsFraction )
{
    //  return ISO time period string
    rtl::OUStringBuffer sTmp;
    sTmp.append( sal_Unicode('P') );                // "period"

    sal_uInt16 nHours = rTime.GetHour();
    sal_Bool bHasHours = ( nHours > 0 );
    if ( nHours >= 24 )
    {
        //  add days

        sal_uInt16 nDays = nHours / 24;
        sTmp.append( (sal_Int32) nDays );
        sTmp.append( sal_Unicode('D') );            // "days"

        nHours -= nDays * 24;
    }
    sTmp.append( sal_Unicode('T') );                // "time"

    if ( bHasHours )
    {
        sTmp.append( (sal_Int32) nHours );
        sTmp.append( sal_Unicode('H') );            // "hours"
    }
    sal_uInt16 nMinutes = rTime.GetMin();
    if ( bHasHours || nMinutes > 0 )
    {
        sTmp.append( (sal_Int32) nMinutes );
        sTmp.append( sal_Unicode('M') );            // "minutes"
    }
    sal_uInt16 nSeconds = rTime.GetSec();
    sTmp.append( (sal_Int32) nSeconds );
    if ( nSecondsFraction )
    {
        sTmp.append( sal_Unicode( '.' ) );
        ::rtl::OUStringBuffer aFractional;
        convertNumber( aFractional, nSecondsFraction );
        sTmp.append( aFractional.getStr() );
    }
    sTmp.append( sal_Unicode('S') );            // "seconds"

    return sTmp.makeStringAndClear();
}

// static
bool SvXMLUnitConverter::convertTimeDuration( const rtl::OUString& rString, Time& rTime, sal_Int32* pSecondsFraction )
{
    rtl::OUString aTrimmed = rString.trim().toAsciiUpperCase();
    const sal_Unicode* pStr = aTrimmed.getStr();

    if ( *(pStr++) != sal_Unicode('P') )            // duration must start with "P"
        return false;

    bool bSuccess = true;
    sal_Bool bDone = sal_False;
    sal_Bool bTimePart = sal_False;
    sal_Bool bFractional = sal_False;
    sal_Int32 nDays  = 0;
    sal_Int32 nHours = 0;
    sal_Int32 nMins  = 0;
    sal_Int32 nSecs  = 0;
    sal_Int32 nTemp = 0;
    sal_Int32 nSecondsFraction = 0;

    while ( bSuccess && !bDone )
    {
        sal_Unicode c = *(pStr++);
        if ( !c )                               // end
            bDone = sal_True;
        else if ( sal_Unicode('0') <= c && sal_Unicode('9') >= c )
        {
            if ( bFractional )
            {
                if ( nSecondsFraction >= SAL_MAX_INT32 / 10 )
                    bSuccess = false;
                else
                {
                    nSecondsFraction *= 10;
                    nSecondsFraction += (c - sal_Unicode('0'));
                }
            }
            else
            {
                if ( nTemp >= SAL_MAX_INT32 / 10 )
                    bSuccess = false;
                else
                {
                    nTemp *= 10;
                    nTemp += (c - sal_Unicode('0'));
                }
            }
        }
        else if ( bTimePart )
        {
            if ( c == sal_Unicode('H') )
            {
                nHours = nTemp;
                nTemp = 0;
            }
            else if ( c == sal_Unicode('M') )
            {
                nMins = nTemp;
                nTemp = 0;
            }
            else if ( c == sal_Unicode('S') )
            {
                nSecs = nTemp;
                nTemp = 0;
            }
            else if ( c == '.' )
            {
                bFractional = sal_True;
            }
            else
                bSuccess = false;               // invalid characted
        }
        else
        {
            if ( c == sal_Unicode('T') )            // "T" starts time part
                bTimePart = sal_True;
            else if ( c == sal_Unicode('D') )
            {
                nDays = nTemp;
                nTemp = 0;
            }
            else if ( c == sal_Unicode('Y') || c == sal_Unicode('M') )
            {
                //! how many days is a year or month?

                DBG_ERROR("years or months in duration: not implemented");
                bSuccess = false;
            }
            else
                bSuccess = false;               // invalid characted
        }
    }

    if ( bSuccess )
    {
        if ( nDays )
            nHours += nDays * 24;               // add the days to the hours part
        rTime = Time( nHours, nMins, nSecs );
        if ( pSecondsFraction )
            *pSecondsFraction = nSecondsFraction % 1000;
    }
    return bSuccess;
}

sal_Bool SvXMLUnitConverter::convertAny(      ::rtl::OUStringBuffer&    sValue,
                                              ::rtl::OUStringBuffer&    sType ,
                                        const com::sun::star::uno::Any& aValue)
{
    sal_Bool bConverted = sal_False;
    
    sValue.setLength(0);
    sType.setLength (0);
    
    switch(aValue.getValueTypeClass())
    {
        case com::sun::star::uno::TypeClass_BYTE :
        case com::sun::star::uno::TypeClass_SHORT :
        case com::sun::star::uno::TypeClass_UNSIGNED_SHORT :
        case com::sun::star::uno::TypeClass_LONG :
        case com::sun::star::uno::TypeClass_UNSIGNED_LONG :
            {
                sal_Int32 nTempValue = 0;
                if (aValue >>= nTempValue)
                {
                    sType.appendAscii("integer");
                    bConverted = sal_True;
                    SvXMLUnitConverter::convertNumber(sValue, nTempValue);
                }
            }
            break;
            
        case com::sun::star::uno::TypeClass_BOOLEAN :
            {
                sal_Bool bTempValue = sal_False;
                if (aValue >>= bTempValue)
                {
                    sType.appendAscii("boolean");
                    bConverted = sal_True;
                    SvXMLUnitConverter::convertBool(sValue, bTempValue);
                }
            }
            break;
            
        case com::sun::star::uno::TypeClass_FLOAT :
        case com::sun::star::uno::TypeClass_DOUBLE :
            {
                double fTempValue = 0.0;
                if (aValue >>= fTempValue)
                {
                    sType.appendAscii("float");
                    bConverted = sal_True; 
                    SvXMLUnitConverter::convertDouble(sValue, fTempValue);
                }
            }
            break;
            
        case com::sun::star::uno::TypeClass_STRING :
            {
                ::rtl::OUString sTempValue;
                if (aValue >>= sTempValue)
                {
                    sType.appendAscii("string");
                    bConverted = sal_True;
                    sValue.append(sTempValue);
                }
            }
            break;
            
        case com::sun::star::uno::TypeClass_STRUCT :
            {
                com::sun::star::util::Date     aDate    ;
                com::sun::star::util::Time     aTime    ;
                com::sun::star::util::DateTime aDateTime;
                
                if (aValue >>= aDate)
                {
                    sType.appendAscii("date");
                    bConverted = sal_True;
                    com::sun::star::util::DateTime aTempValue;
                    aTempValue.Day              = aDate.Day;
                    aTempValue.Month            = aDate.Month;
                    aTempValue.Year             = aDate.Year;
                    aTempValue.HundredthSeconds = 0;
                    aTempValue.Seconds          = 0;
                    aTempValue.Minutes          = 0;
                    aTempValue.Hours            = 0;
                    SvXMLUnitConverter::convertDateTime(sValue, aTempValue);
                }
                else
                if (aValue >>= aTime)
                {
                    sType.appendAscii("time");
                    bConverted = sal_True;
                    com::sun::star::util::DateTime aTempValue;
                    aTempValue.Day              = 0;
                    aTempValue.Month            = 0;
                    aTempValue.Year             = 0;
                    aTempValue.HundredthSeconds = aTime.HundredthSeconds;
                    aTempValue.Seconds          = aTime.Seconds;
                    aTempValue.Minutes          = aTime.Minutes;
                    aTempValue.Hours            = aTime.Hours;
                    SvXMLUnitConverter::convertTime(sValue, aTempValue);
                }
                else
                if (aValue >>= aDateTime)
                {
                    sType.appendAscii("date");
                    bConverted = sal_True;
                    SvXMLUnitConverter::convertDateTime(sValue, aDateTime);
                }
            }
            break;
		default:
			break;
    }
    
    return bConverted;
}
                       
sal_Bool SvXMLUnitConverter::convertAny(      com::sun::star::uno::Any& aValue,
                                        const ::rtl::OUString&          sType ,
                                        const ::rtl::OUString&          sValue)
{
    sal_Bool bConverted = sal_False;
    
    if (sType.equalsAscii("boolean"))
    {
        sal_Bool bTempValue = sal_False;
        SvXMLUnitConverter::convertBool(bTempValue, sValue);
        aValue <<= bTempValue;
        bConverted = sal_True;
    }
    else    
    if (sType.equalsAscii("integer"))
    {
        sal_Int32 nTempValue = 0;
        SvXMLUnitConverter::convertNumber(nTempValue, sValue);
        aValue <<= nTempValue;
        bConverted = sal_True;
    }
    else    
    if (sType.equalsAscii("float"))
    {
        double fTempValue = 0.0;
        SvXMLUnitConverter::convertDouble(fTempValue, sValue);
        aValue <<= fTempValue;
        bConverted = sal_True;
    }
    else    
    if (sType.equalsAscii("string"))
    {
        aValue <<= sValue;
        bConverted = sal_True;
    }
    else
    if (sType.equalsAscii("date"))
    {
        com::sun::star::util::DateTime aTempValue;
        SvXMLUnitConverter::convertDateTime(aTempValue, sValue);
        aValue <<= aTempValue;
        bConverted = sal_True;
    }
    else
    if (sType.equalsAscii("time"))
    {
        com::sun::star::util::DateTime aTempValue;
        com::sun::star::util::Time     aConvValue;
        SvXMLUnitConverter::convertTime(aTempValue, sValue);
        aConvValue.HundredthSeconds = aTempValue.HundredthSeconds;
        aConvValue.Seconds          = aTempValue.Seconds;
        aConvValue.Minutes          = aTempValue.Minutes;
        aConvValue.Hours            = aTempValue.Hours;
        aValue <<= aConvValue;
        bConverted = sal_True;
    }
    
    return bConverted;
}
