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



#include "precompiled_vcl.hxx"

#include "svdata.hxx"

#include "vcl/arrange.hxx"
#include "vcl/edit.hxx"
#include "vcl/svapp.hxx"

#include "com/sun/star/beans/PropertyValue.hpp"
#include "com/sun/star/awt/Rectangle.hpp"

#include "osl/diagnose.h"

using namespace vcl;
using namespace com::sun::star;

// ----------------------------------------
// vcl::WindowArranger
//-----------------------------------------

long WindowArranger::getDefaultBorder()
{
    ImplSVData* pSVData = ImplGetSVData();
    long nResult = pSVData->maAppData.mnDefaultLayoutBorder;
    if( nResult < 0 )
    {
        OutputDevice* pDefDev = Application::GetDefaultDevice();
        if( pDefDev )
        {
            Size aBorder( pDefDev->LogicToPixel( Size( 3, 3 ), MapMode( MAP_APPFONT ) ) );
            nResult = pSVData->maAppData.mnDefaultLayoutBorder = aBorder.Height();
        }
    }
    return nResult > 0 ? nResult : 0;
}

WindowArranger::~WindowArranger()
{}

void WindowArranger::setParent( WindowArranger* i_pParent )
{
    OSL_VERIFY( i_pParent->m_pParentWindow == m_pParentWindow || m_pParentWindow == NULL );

    m_pParentArranger = i_pParent;
    m_pParentWindow = i_pParent->m_pParentWindow;
    setParentWindow( m_pParentWindow );
}

void WindowArranger::setParentWindow( Window* i_pNewParent )
{
    m_pParentWindow = i_pNewParent;
    
    size_t nEle = countElements();
    for( size_t i = 0; i < nEle; i++ )
    {
        Element* pEle = getElement( i );
        if( pEle ) // sanity check
        {
            #if OSL_DEBUG_LEVEL > 0
            if( pEle->m_pElement )
            {
                OSL_VERIFY( pEle->m_pElement->GetParent() == i_pNewParent );
            }
            #endif
            if( pEle->m_pChild )
                pEle->m_pChild->setParentWindow( i_pNewParent );
        }
    }
}

void WindowArranger::show( bool i_bShow, bool i_bImmediateUpdate )
{
    size_t nEle = countElements();
    for( size_t i = 0; i < nEle; i++ )
    {
        Element* pEle = getElement( i );
        if( pEle ) // sanity check
        {
            pEle->m_bHidden = ! i_bShow;
            if( pEle->m_pElement )
                pEle->m_pElement->Show( i_bShow );
            if( pEle->m_pChild.get() )
                pEle->m_pChild->show( i_bShow, false );
        }
    }
    if( m_pParentArranger )
    {
        nEle = m_pParentArranger->countElements();
        for( size_t i = 0; i < nEle; i++ )
        {
            Element* pEle = m_pParentArranger->getElement( i );
            if( pEle && pEle->m_pChild.get() == this )
            {
                pEle->m_bHidden = ! i_bShow;
                break;
            }
        }
    }
    if( i_bImmediateUpdate )
    {
        // find the topmost parent
        WindowArranger* pResize = this;
        while( pResize->m_pParentArranger )
            pResize = pResize->m_pParentArranger;
        pResize->resize();
    }
}

bool WindowArranger::isVisible() const
{
    size_t nEle = countElements();
    for( size_t i = 0; i < nEle; i++ )
    {
        const Element* pEle = getConstElement( i );
        if( pEle->isVisible() )
            return true;
    }
    return false;
}

bool WindowArranger::Element::isVisible() const
{
    bool bVisible = false;
    if( ! m_bHidden )
    {
        if( m_pElement )
            bVisible = m_pElement->IsVisible();
        else if( m_pChild )
            bVisible = m_pChild->isVisible();
    }
    return bVisible;
}

sal_Int32 WindowArranger::Element::getExpandPriority() const
{
    sal_Int32 nPrio = m_nExpandPriority;
    if( m_pChild && m_nExpandPriority >= 0 )
    {
        size_t nElements = m_pChild->countElements();
        for( size_t i = 0; i < nElements; i++ )
        {
            sal_Int32 nCPrio = m_pChild->getExpandPriority( i );
            if( nCPrio > nPrio )
                nPrio = nCPrio;
        }
    }
    return nPrio;
}

Size WindowArranger::Element::getOptimalSize( WindowSizeType i_eType ) const
{
    Size aResult;
    if( ! m_bHidden )
    {
        bool bVisible = false;
        if( m_pElement && m_pElement->IsVisible() )
        {
            aResult = m_pElement->GetOptimalSize( i_eType );
            bVisible = true;
        }
        else if( m_pChild && m_pChild->isVisible() )
        {
            aResult = m_pChild->getOptimalSize( i_eType );
            bVisible = true;
        }
        if( bVisible )
        {
            if( aResult.Width() < m_aMinSize.Width() )
                aResult.Width() = m_aMinSize.Width();
            if( aResult.Height() < m_aMinSize.Height() )
                aResult.Height() = m_aMinSize.Height();
            aResult.Width() += getBorderValue( m_nLeftBorder ) + getBorderValue( m_nRightBorder );
            aResult.Height() += getBorderValue( m_nTopBorder ) + getBorderValue( m_nBottomBorder );
        }
    }
    
    return aResult;
}

void WindowArranger::Element::setPosSize( const Point& i_rPos, const Size& i_rSize )
{
    Point aPoint( i_rPos );
    Size aSize( i_rSize );
    aPoint.X() += getBorderValue( m_nLeftBorder );
    aPoint.Y() += getBorderValue( m_nTopBorder );
    aSize.Width() -= getBorderValue( m_nLeftBorder ) + getBorderValue( m_nRightBorder );
    aSize.Height() -= getBorderValue( m_nTopBorder ) + getBorderValue( m_nBottomBorder );
    if( m_pElement )
        m_pElement->SetPosSizePixel( aPoint, aSize );
    else if( m_pChild )
        m_pChild->setManagedArea( Rectangle( aPoint, aSize ) );
}

uno::Sequence< beans::PropertyValue > WindowArranger::getProperties() const
{
    uno::Sequence< beans::PropertyValue > aRet( 3 );
    aRet[0].Name  = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "OuterBorder" ) );
    aRet[0].Value = uno::makeAny( sal_Int32( getBorderValue( m_nOuterBorder ) ) );
    aRet[1].Name  = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "ManagedArea" ) );
    awt::Rectangle aArea( m_aManagedArea.getX(), m_aManagedArea.getY(), m_aManagedArea.getWidth(), m_aManagedArea.getHeight() );
    aRet[1].Value = uno::makeAny( aArea );
    aRet[2].Name  = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "Visible" ) );
    aRet[2].Value = uno::makeAny( sal_Bool( isVisible() ) );
    return aRet;
}

void WindowArranger::setProperties( const uno::Sequence< beans::PropertyValue >& i_rProps )
{
    const beans::PropertyValue* pProps = i_rProps.getConstArray();
    bool bResize = false;
    for( sal_Int32 i = 0; i < i_rProps.getLength(); i++ )
    {
        if( pProps[i].Name.equalsAscii( "OuterBorder" ) )
        {
            sal_Int32 nVal = 0;
            if( pProps[i].Value >>= nVal )
            {
                if( getBorderValue( m_nOuterBorder ) != nVal )
                {
                    m_nOuterBorder = nVal;
                    bResize = true;
                }
            }
        }
        else if( pProps[i].Name.equalsAscii( "ManagedArea" ) )
        {
            awt::Rectangle aArea( 0, 0, 0, 0 );
            if( pProps[i].Value >>= aArea )
            {
                m_aManagedArea.setX( aArea.X );
                m_aManagedArea.setY( aArea.Y );
                m_aManagedArea.setWidth( aArea.Width );
                m_aManagedArea.setHeight( aArea.Height );
                bResize = true;
            }
        }
        else if( pProps[i].Name.equalsAscii( "Visible" ) )
        {
            sal_Bool bVal = sal_False;
            if( pProps[i].Value >>= bVal )
            {
                show( bVal, false );
                bResize = true;
            }
        }
    }
    if( bResize )
        resize();
}


// ----------------------------------------
// vcl::RowOrColumn
//-----------------------------------------

RowOrColumn::~RowOrColumn()
{
    for( std::vector< WindowArranger::Element >::iterator it = m_aElements.begin();
         it != m_aElements.end(); ++it )
    {
        it->deleteChild();
    }
}

Size RowOrColumn::getOptimalSize( WindowSizeType i_eType ) const
{
    Size aRet( 0, 0 );
    long nDistance = getBorderValue( m_nBorderWidth );
    for( std::vector< WindowArranger::Element >::const_iterator it = m_aElements.begin();
         it != m_aElements.end(); ++it )
    {
        if( it->isVisible() )
        {
            // get the size of type of the managed element
            Size aElementSize( it->getOptimalSize( i_eType ) );
            if( m_bColumn )
            {
                // add the distance between elements
                aRet.Height() += nDistance;
                // check if the width needs adjustment
                if( aRet.Width() < aElementSize.Width() )
                    aRet.Width() = aElementSize.Width();
                aRet.Height() += aElementSize.Height();
            }
            else
            {
                // add the distance between elements
                aRet.Width() += nDistance;
                // check if the height needs adjustment
                if( aRet.Height() < aElementSize.Height() )
                    aRet.Height() = aElementSize.Height();
                aRet.Width() += aElementSize.Width();
            }
        }
    }

    if( aRet.Width() != 0 || aRet.Height() != 0 )
    {
        // subtract the border for the first element
        if( m_bColumn )
            aRet.Height() -= nDistance;
        else
            aRet.Width() -= nDistance;
    
        // add the outer border
        long nOuterBorder = getBorderValue( m_nOuterBorder );
        aRet.Width() += 2*nOuterBorder;
        aRet.Height() += 2*nOuterBorder;
    }

    return aRet;
}

void RowOrColumn::distributeRowWidth( std::vector<Size>& io_rSizes, long /*i_nUsedWidth*/, long i_nExtraWidth )
{
    if( ! io_rSizes.empty() && io_rSizes.size() == m_aElements.size() )
    {
        // find all elements with the highest expand priority
        size_t nElements = m_aElements.size();
        std::vector< size_t > aIndices;
        sal_Int32 nHighPrio = 0;
        for( size_t i = 0; i < nElements; i++ )
        {
            if( m_aElements[ i ].isVisible() )
            {
                sal_Int32 nCurPrio = m_aElements[ i ].getExpandPriority();
                if( nCurPrio > nHighPrio )
                {
                    aIndices.clear();
                    nHighPrio = nCurPrio;
                }
                if( nCurPrio == nHighPrio )
                    aIndices.push_back( i );
            }
        }
        
        // distribute extra space evenly among collected elements
        nElements = aIndices.size();
        if( nElements > 0 )
        {
            long nDelta = i_nExtraWidth / nElements;
            for( size_t i = 0; i < nElements; i++ )
            {
                io_rSizes[ aIndices[i] ].Width() += nDelta;
                i_nExtraWidth -= nDelta;
            }
            // add the last pixels to the last row element
            if( i_nExtraWidth > 0 && nElements > 0 )
                io_rSizes[aIndices.back()].Width() += i_nExtraWidth;
        }
    }
}

void RowOrColumn::distributeColumnHeight( std::vector<Size>& io_rSizes, long /*i_nUsedHeight*/, long i_nExtraHeight )
{
    if( ! io_rSizes.empty() && io_rSizes.size() == m_aElements.size() )
    {
        // find all elements with the highest expand priority
        size_t nElements = m_aElements.size();
        std::vector< size_t > aIndices;
        sal_Int32 nHighPrio = 3;
        for( size_t i = 0; i < nElements; i++ )
        {
            if( m_aElements[ i ].isVisible() )
            {
                sal_Int32 nCurPrio = m_aElements[ i ].getExpandPriority();
                if( nCurPrio > nHighPrio )
                {
                    aIndices.clear();
                    nHighPrio = nCurPrio;
                }
                if( nCurPrio == nHighPrio )
                    aIndices.push_back( i );
            }
        }
        
        // distribute extra space evenly among collected elements
        nElements = aIndices.size();
        if( nElements > 0 )
        {
            long nDelta = i_nExtraHeight / nElements;
            for( size_t i = 0; i < nElements; i++ )
            {
                io_rSizes[ aIndices[i] ].Height() += nDelta;
                i_nExtraHeight -= nDelta;
            }
            // add the last pixels to the last row element
            if( i_nExtraHeight > 0 && nElements > 0 )
                io_rSizes[aIndices.back()].Height() += i_nExtraHeight;
        }
    }
}

void RowOrColumn::resize()
{
    // check if we can get optimal size, else fallback to minimal size
    Size aOptSize( getOptimalSize( WINDOWSIZE_PREFERRED ) );
    WindowSizeType eType = WINDOWSIZE_PREFERRED;
    if( m_bColumn )
    {
        if( aOptSize.Height() > m_aManagedArea.GetHeight() )
            eType = WINDOWSIZE_MINIMUM;
    }
    else
    {
        if( aOptSize.Width() > m_aManagedArea.GetWidth() )
            eType = WINDOWSIZE_MINIMUM;
    }

    size_t nElements = m_aElements.size();
    // get all element sizes for sizing
    std::vector<Size> aElementSizes( nElements );
    long nDistance = getBorderValue( m_nBorderWidth );
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    long nUsedWidth = 2*nOuterBorder - (nElements ? nDistance : 0);
    for( size_t i = 0; i < nElements; i++ )
    {
        if( m_aElements[i].isVisible() )
        {
            aElementSizes[i] = m_aElements[i].getOptimalSize( eType );
            if( m_bColumn )
            {
                aElementSizes[i].Width() = m_aManagedArea.GetWidth() - 2 * nOuterBorder;
                nUsedWidth += aElementSizes[i].Height() + nDistance;
            }
            else
            {
                aElementSizes[i].Height() = m_aManagedArea.GetHeight() - 2 * nOuterBorder;
                nUsedWidth += aElementSizes[i].Width() + nDistance;
            }
        }
    }

    long nExtraWidth = (m_bColumn ? m_aManagedArea.GetHeight() : m_aManagedArea.GetWidth()) - nUsedWidth;
    if( nExtraWidth > 0 )
    {
        if( m_bColumn )
            distributeColumnHeight( aElementSizes, nUsedWidth, nExtraWidth );
        else
            distributeRowWidth( aElementSizes, nUsedWidth, nExtraWidth );
    }

    // get starting position
    Point aElementPos( m_aManagedArea.TopLeft() );
    // outer border
    aElementPos.X() += nOuterBorder;
    aElementPos.Y() += nOuterBorder;

    // position managed windows
    for( size_t i = 0; i < nElements; i++ )
    {
        // get the size of type of the managed element
        if( m_aElements[i].isVisible() )
        {
            m_aElements[i].setPosSize( aElementPos, aElementSizes[i] );
            if( m_bColumn )
                aElementPos.Y() += nDistance + aElementSizes[i].Height();
            else
                aElementPos.X() += nDistance + aElementSizes[i].Width();
        }
    }
}

size_t RowOrColumn::addWindow( Window* i_pWindow, sal_Int32 i_nExpandPrio, const Size& i_rMinSize, size_t i_nIndex )
{
    size_t nIndex = i_nIndex;
    if( i_nIndex >= m_aElements.size() )
    {
        nIndex = m_aElements.size();
        m_aElements.push_back( WindowArranger::Element( i_pWindow, boost::shared_ptr<WindowArranger>(), i_nExpandPrio, i_rMinSize ) );
    }
    else
    {
        std::vector< WindowArranger::Element >::iterator it = m_aElements.begin();
        while( i_nIndex-- )
            ++it;
        m_aElements.insert( it, WindowArranger::Element( i_pWindow, boost::shared_ptr<WindowArranger>(), i_nExpandPrio, i_rMinSize ) );
    }
    return nIndex;
}

size_t RowOrColumn::addChild( boost::shared_ptr<WindowArranger> const & i_pChild, sal_Int32 i_nExpandPrio, size_t i_nIndex )
{
    size_t nIndex = i_nIndex;
    if( i_nIndex >= m_aElements.size() )
    {
        nIndex = m_aElements.size();
        m_aElements.push_back( WindowArranger::Element( NULL, i_pChild, i_nExpandPrio ) );
    }
    else
    {
        std::vector< WindowArranger::Element >::iterator it = m_aElements.begin();
        while( i_nIndex-- )
            ++it;
        m_aElements.insert( it, WindowArranger::Element( NULL, i_pChild, i_nExpandPrio ) );
    }
    return nIndex;
}

void RowOrColumn::remove( Window* i_pWindow )
{
    if( i_pWindow )
    {
        for( std::vector< WindowArranger::Element >::iterator it = m_aElements.begin();
            it != m_aElements.end(); ++it )
        {
            if( it->m_pElement == i_pWindow )
            {
                m_aElements.erase( it );
                return;
            }
        }
    }
}

void RowOrColumn::remove( boost::shared_ptr<WindowArranger> const & i_pChild )
{
    if( i_pChild )
    {
        for( std::vector< WindowArranger::Element >::iterator it = m_aElements.begin();
            it != m_aElements.end(); ++it )
        {
            if( it->m_pChild == i_pChild )
            {
                m_aElements.erase( it );
                return;
            }
        }
    }
}

// ----------------------------------------
// vcl::LabeledElement
//-----------------------------------------

LabeledElement::~LabeledElement()
{
    m_aLabel.deleteChild();
    m_aElement.deleteChild();
}

Size LabeledElement::getOptimalSize( WindowSizeType i_eType ) const
{
    Size aRet( m_aLabel.getOptimalSize( WINDOWSIZE_MINIMUM ) );
    if( aRet.Width() != 0 )
    {
        if( m_nLabelColumnWidth != 0 )
            aRet.Width() = m_nLabelColumnWidth;
        else
            aRet.Width() += getBorderValue( m_nDistance );
    }
    Size aElementSize( m_aElement.getOptimalSize( i_eType ) );
    aRet.Width() += aElementSize.Width();
    if( aElementSize.Height() > aRet.Height() )
        aRet.Height() = aElementSize.Height();
    if( aRet.Height() != 0 )
        aRet.Height() += 2 * getBorderValue( m_nOuterBorder );

    return aRet;
}

void LabeledElement::resize()
{
    Size aLabelSize( m_aLabel.getOptimalSize( WINDOWSIZE_MINIMUM ) );
    Size aElementSize( m_aElement.getOptimalSize( WINDOWSIZE_PREFERRED ) );
    long nDistance = getBorderValue( m_nDistance );
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    if( nDistance + aLabelSize.Width() + aElementSize.Width() > m_aManagedArea.GetWidth() )
        aElementSize = m_aElement.getOptimalSize( WINDOWSIZE_MINIMUM );
    
    // align label and element vertically in LabeledElement
    long nYOff = (m_aManagedArea.GetHeight() - 2*nOuterBorder - aLabelSize.Height()) / 2;
    Point aPos( m_aManagedArea.Left(),
                m_aManagedArea.Top() + nOuterBorder + nYOff );
    Size aSize( aLabelSize );
    if( m_nLabelColumnWidth != 0 )
        aSize.Width() = m_nLabelColumnWidth;
    m_aLabel.setPosSize( aPos, aSize );
    
    aPos.X() += aSize.Width() + nDistance;
    nYOff = (m_aManagedArea.GetHeight() - 2*nOuterBorder - aElementSize.Height()) / 2;
    aPos.Y() = m_aManagedArea.Top() + nOuterBorder + nYOff;
    aSize.Width() = aElementSize.Width();
    aSize.Height() = m_aManagedArea.GetHeight() - 2*nOuterBorder;
    
    // label style
    // 0: position left and right
    // 1: keep the element close to label and grow it
    // 2: keep the element close and don't grow it
    if( m_nLabelStyle == 0)
    {
        if( aPos.X() + aSize.Width() < m_aManagedArea.Right() )
            aPos.X() = m_aManagedArea.Right() - aSize.Width();
    }
    else if( m_nLabelStyle == 1 )
    {
        if( aPos.X() + aSize.Width() < m_aManagedArea.Right() )
            aSize.Width() = m_aManagedArea.Right() - aPos.X();
    }
    m_aElement.setPosSize( aPos, aSize );
}

void LabeledElement::setLabel( Window* i_pLabel )
{
    m_aLabel.m_pElement = i_pLabel;
    m_aLabel.m_pChild.reset();
}

void LabeledElement::setLabel( boost::shared_ptr<WindowArranger> const & i_pLabel )
{
    m_aLabel.m_pElement = NULL;
    m_aLabel.m_pChild = i_pLabel;
}

void LabeledElement::setElement( Window* i_pElement )
{
    m_aElement.m_pElement = i_pElement;
    m_aElement.m_pChild.reset();
}

void LabeledElement::setElement( boost::shared_ptr<WindowArranger> const & i_pElement )
{
    m_aElement.m_pElement = NULL;
    m_aElement.m_pChild = i_pElement;
}

// ----------------------------------------
// vcl::LabelColumn
//-----------------------------------------
LabelColumn::~LabelColumn()
{
}

long LabelColumn::getLabelWidth() const
{
    long nWidth = 0;

    size_t nEle = countElements();
    for( size_t i = 0; i < nEle; i++ )
    {
        const Element* pEle = getConstElement( i );
        if( pEle && pEle->m_pChild.get() )
        {
            const LabeledElement* pLabel = dynamic_cast< const LabeledElement* >(pEle->m_pChild.get());
            if( pLabel )
            {
                Window* pLW = pLabel->getWindow( 0 );
                if( pLW )
                {
                    Size aLabSize( pLW->GetOptimalSize( WINDOWSIZE_MINIMUM ) );
                    long nLB = 0;
                    pLabel->getBorders(0, &nLB);
                    aLabSize.Width() += getBorderValue( nLB );
                    if( aLabSize.Width() > nWidth )
                        nWidth = aLabSize.Width();
                }
            }
        }
    }
    return nWidth + getBorderValue( getBorderWidth() );
}

Size LabelColumn::getOptimalSize( WindowSizeType i_eType ) const
{
    long nWidth = getLabelWidth();
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    Size aColumnSize;
    
    // every child is a LabeledElement
    size_t nEle = countElements();
    for( size_t i = 0; i < nEle; i++ )
    {
        Size aElementSize;
        const Element* pEle = getConstElement( i );
        if( pEle && pEle->m_pChild.get() )
        {
            const LabeledElement* pLabel = dynamic_cast< const LabeledElement* >(pEle->m_pChild.get());
            if( pLabel ) // we have a label
            {
                aElementSize = pLabel->getLabelSize( WINDOWSIZE_MINIMUM );
                if( aElementSize.Width() )
                    aElementSize.Width() = nWidth;
                Size aSize( pLabel->getElementSize( i_eType ) );
                aElementSize.Width() += aSize.Width();
                if( aSize.Height() > aElementSize.Height() )
                    aElementSize.Height() = aSize.Height();
            }
            else // a non label, just treat it as a row
            {
                aElementSize = pEle->getOptimalSize( i_eType );
            }
        }
        else if( pEle && pEle->m_pElement ) // a general window, treat is as a row
        {
            aElementSize = pEle->getOptimalSize( i_eType );
        }
        if( aElementSize.Width() )
        {
            aElementSize.Width() += 2*nOuterBorder;
            if( aElementSize.Width() > aColumnSize.Width() )
                aColumnSize.Width() = aElementSize.Width();
        }
        if( aElementSize.Height() )
        {
            aColumnSize.Height() += getBorderValue( getBorderWidth() ) + aElementSize.Height(); 
        }
    }
    if( nEle > 0 && aColumnSize.Height() )
    {
        aColumnSize.Height() -= getBorderValue( getBorderWidth() ); // for the first element
        aColumnSize.Height() += 2*nOuterBorder;
    }
    return aColumnSize;
}

void LabelColumn::resize()
{
    long nWidth = getLabelWidth();
    size_t nEle = countElements();
    for( size_t i = 0; i < nEle; i++ )
    {
        Element* pEle = getElement( i );
        if( pEle && pEle->m_pChild.get() )
        {
            LabeledElement* pLabel = dynamic_cast< LabeledElement* >(pEle->m_pChild.get());
            if( pLabel )
                pLabel->setLabelColumnWidth( nWidth );
        }
    }
    RowOrColumn::resize();
}

size_t LabelColumn::addRow( Window* i_pLabel, boost::shared_ptr<WindowArranger> const& i_rElement, long i_nIndent )
{
    boost::shared_ptr< LabeledElement > xLabel( new LabeledElement( this, 1 ) );
    xLabel->setLabel( i_pLabel );
    xLabel->setBorders( 0, i_nIndent, 0, 0, 0 );
    xLabel->setElement( i_rElement );
    size_t nIndex = addChild( xLabel );
    resize();
    return nIndex;
}

size_t LabelColumn::addRow( Window* i_pLabel, Window* i_pElement, long i_nIndent, const Size& i_rElementMinSize )
{
    boost::shared_ptr< LabeledElement > xLabel( new LabeledElement( this, 1 ) );
    xLabel->setLabel( i_pLabel );
    xLabel->setBorders( 0, i_nIndent, 0, 0, 0 );
    xLabel->setElement( i_pElement );
    xLabel->setMinimumSize( 1, i_rElementMinSize );
    size_t nIndex = addChild( xLabel );
    resize();
    return nIndex;
}

// ----------------------------------------
// vcl::Indenter
//-----------------------------------------

Indenter::~Indenter()
{
    m_aElement.deleteChild();
}

Size Indenter::getOptimalSize( WindowSizeType i_eType ) const
{
    Size aSize( m_aElement.getOptimalSize( i_eType ) );
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    long nIndent = getBorderValue( m_nIndent );
    aSize.Width()  += 2*nOuterBorder + nIndent;
    aSize.Height() += 2*nOuterBorder;
    return aSize;
}

void Indenter::resize()
{
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    long nIndent = getBorderValue( m_nIndent );
    Point aPt( m_aManagedArea.TopLeft() );
    aPt.X() += nOuterBorder + nIndent;
    aPt.Y() += nOuterBorder;
    Size aSz( m_aManagedArea.GetSize() );
    aSz.Width()  -= 2*nOuterBorder + nIndent;
    aSz.Height() -= 2*nOuterBorder;
    m_aElement.setPosSize( aPt, aSz );
}

void Indenter::setWindow( Window* i_pWindow, sal_Int32 i_nExpandPrio )
{
    OSL_VERIFY( (m_aElement.m_pElement == 0 && m_aElement.m_pChild == 0) || i_pWindow == 0 );
    OSL_VERIFY( i_pWindow == 0 || i_pWindow->GetParent() == m_pParentWindow );
    m_aElement.m_pElement = i_pWindow;
    m_aElement.m_nExpandPriority = i_nExpandPrio;
}

void Indenter::setChild( boost::shared_ptr<WindowArranger> const & i_pChild, sal_Int32 i_nExpandPrio )
{
    OSL_VERIFY( (m_aElement.m_pElement == 0 && m_aElement.m_pChild == 0 ) || i_pChild == 0 );
    m_aElement.m_pChild = i_pChild;
    m_aElement.m_nExpandPriority = i_nExpandPrio;
}

// ----------------------------------------
// vcl::MatrixArranger
//-----------------------------------------
MatrixArranger::~MatrixArranger()
{
}

Size MatrixArranger::getOptimalSize( WindowSizeType i_eType,
                                     std::vector<long>& o_rColumnWidths, std::vector<long>& o_rRowHeights,
                                     std::vector<sal_Int32>& o_rColumnPrio, std::vector<sal_Int32>& o_rRowPrio
                                    ) const
{
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    Size aMatrixSize( 2*nOuterBorder, 2*nOuterBorder );
    
    // first find out the current number of rows and columns
    sal_uInt32 nRows = 0, nColumns = 0;
    for( std::vector< MatrixElement >::const_iterator it = m_aElements.begin();
            it != m_aElements.end(); ++it )
    {
        if( it->m_nX >= nColumns )
            nColumns = it->m_nX+1;
        if( it->m_nY >= nRows )
            nRows = it->m_nY+1;
    }
    
    // now allocate row and column depth vectors
    o_rColumnWidths = std::vector< long >( nColumns, 0 );
    o_rRowHeights   = std::vector< long >( nRows, 0 );
    o_rColumnPrio   = std::vector< sal_Int32 >( nColumns, 0 );
    o_rRowPrio      = std::vector< sal_Int32 >( nRows, 0 );
    
    // get sizes an allocate them into rows/columns
    for( std::vector< MatrixElement >::const_iterator it = m_aElements.begin();
            it != m_aElements.end(); ++it )
    {
        Size aSize( it->getOptimalSize( i_eType ) );
        if( aSize.Width() > o_rColumnWidths[ it->m_nX ] )
            o_rColumnWidths[ it->m_nX ] = aSize.Width();
        if( aSize.Height() > o_rRowHeights[ it->m_nY ] )
            o_rRowHeights[ it->m_nY ] = aSize.Height();
        if( it->m_nExpandPriority > o_rColumnPrio[ it->m_nX ] )
            o_rColumnPrio[ it->m_nX ] = it->m_nExpandPriority;
        if( it->m_nExpandPriority > o_rRowPrio[ it->m_nY ] )
            o_rRowPrio[ it->m_nY ] = it->m_nExpandPriority;
    }
    
    // add up sizes
    long nDistanceX = getBorderValue( m_nBorderX );
    long nDistanceY = getBorderValue( m_nBorderY );
    for( sal_uInt32 i = 0; i < nColumns; i++ )
        aMatrixSize.Width() += o_rColumnWidths[i] + nDistanceX;
    if( nColumns > 0 )
        aMatrixSize.Width() -= nDistanceX;
    
    for( sal_uInt32 i = 0; i < nRows; i++ )
        aMatrixSize.Height() += o_rRowHeights[i] + nDistanceY;
    if( nRows > 0 )
        aMatrixSize.Height() -= nDistanceY;
    
    return aMatrixSize;
}

Size MatrixArranger::getOptimalSize( WindowSizeType i_eType ) const
{
    std::vector<long> aColumnWidths, aRowHeights;
    std::vector<sal_Int32> aColumnPrio, aRowPrio;
    return getOptimalSize( i_eType, aColumnWidths, aRowHeights, aColumnPrio, aRowPrio );
}

void MatrixArranger::distributeExtraSize( std::vector<long>& io_rSizes, const std::vector<sal_Int32>& i_rPrios, long i_nExtraWidth )
{
    if( ! io_rSizes.empty()  && io_rSizes.size() == i_rPrios.size() ) // sanity check
    {
        // find all elements with the highest expand priority
        size_t nElements = io_rSizes.size();
        std::vector< size_t > aIndices;
        sal_Int32 nHighPrio = 0;
        for( size_t i = 0; i < nElements; i++ )
        {
            sal_Int32 nCurPrio = i_rPrios[ i ];
            if( nCurPrio > nHighPrio )
            {
                aIndices.clear();
                nHighPrio = nCurPrio;
            }
            if( nCurPrio == nHighPrio )
                aIndices.push_back( i );
        }
        
        // distribute extra space evenly among collected elements
        nElements = aIndices.size();
        if( nElements > 0 )
        {
            long nDelta = i_nExtraWidth / nElements;
            for( size_t i = 0; i < nElements; i++ )
            {
                io_rSizes[ aIndices[i] ] += nDelta;
                i_nExtraWidth -= nDelta;
            }
            // add the last pixels to the last row element
            if( i_nExtraWidth > 0 && nElements > 0 )
                io_rSizes[aIndices.back()] += i_nExtraWidth;
        }
    }
}


void MatrixArranger::resize()
{
    // assure that we have at least one row and column
    if( m_aElements.empty() )
        return;
    
    // check if we can get optimal size, else fallback to minimal size
    std::vector<long> aColumnWidths, aRowHeights;
    std::vector<sal_Int32> aColumnPrio, aRowPrio;
    Size aOptSize( getOptimalSize( WINDOWSIZE_PREFERRED, aColumnWidths, aRowHeights, aColumnPrio, aRowPrio ) );
    if( aOptSize.Height() > m_aManagedArea.GetHeight() ||
        aOptSize.Width() > m_aManagedArea.GetWidth() )
    {
        std::vector<long> aMinColumnWidths, aMinRowHeights;
        getOptimalSize( WINDOWSIZE_MINIMUM, aMinColumnWidths, aMinRowHeights, aColumnPrio, aRowPrio );
        if( aOptSize.Height() > m_aManagedArea.GetHeight() )
            aRowHeights = aMinRowHeights;
        if( aOptSize.Width() > m_aManagedArea.GetWidth() )
            aColumnWidths = aMinColumnWidths;
    }
    
    // distribute extra space available
    long nExtraSize = m_aManagedArea.GetWidth();
    for( size_t i = 0; i < aColumnWidths.size(); ++i )
        nExtraSize -= aColumnWidths[i] + m_nBorderX;
    if( nExtraSize > 0 )
        distributeExtraSize( aColumnWidths, aColumnPrio, nExtraSize );
    nExtraSize =  m_aManagedArea.GetHeight();
    for( size_t i = 0; i < aRowHeights.size(); ++i )
        nExtraSize -= aRowHeights[i] + m_nBorderY;
    if( nExtraSize > 0 )
        distributeExtraSize( aRowHeights, aRowPrio, nExtraSize );
    
    // prepare offsets
    long nDistanceX = getBorderValue( m_nBorderX );
    long nDistanceY = getBorderValue( m_nBorderY );
    long nOuterBorder = getBorderValue( m_nOuterBorder );
    std::vector<long> aColumnX( aColumnWidths.size() );
    aColumnX[0] = m_aManagedArea.Left() + nOuterBorder;
    for( size_t i = 1; i < aColumnX.size(); i++ )
        aColumnX[i] = aColumnX[i-1] + aColumnWidths[i-1] + nDistanceX;
    
    std::vector<long> aRowY( aRowHeights.size() );
    aRowY[0] = m_aManagedArea.Top() + nOuterBorder;
    for( size_t i = 1; i < aRowY.size(); i++ )
        aRowY[i] = aRowY[i-1] + aRowHeights[i-1] + nDistanceY;
    
    // now iterate over the elements and assign their positions
    for( std::vector< MatrixElement >::iterator it = m_aElements.begin();
         it != m_aElements.end(); ++it )
    {
        Point aCellPos( aColumnX[it->m_nX], aRowY[it->m_nY] );
        Size  aCellSize( aColumnWidths[it->m_nX], aRowHeights[it->m_nY] );
        it->setPosSize( aCellPos, aCellSize );
    }
}

size_t MatrixArranger::addWindow( Window* i_pWindow, sal_uInt32 i_nX, sal_uInt32 i_nY, sal_Int32 i_nExpandPrio, const Size& i_rMinSize )
{
    sal_uInt64 nMapValue = getMap( i_nX, i_nY );
    std::map< sal_uInt64, size_t >::const_iterator it = m_aMatrixMap.find( nMapValue );
    size_t nIndex = 0;
    if( it == m_aMatrixMap.end() )
    {
        m_aMatrixMap[ nMapValue ] = nIndex = m_aElements.size();
        m_aElements.push_back( MatrixElement( i_pWindow, i_nX, i_nY, boost::shared_ptr<WindowArranger>(), i_nExpandPrio, i_rMinSize ) );
    }
    else
    {
        MatrixElement& rEle( m_aElements[ it->second ] );
        rEle.m_pElement = i_pWindow;
        rEle.m_pChild.reset();
        rEle.m_nExpandPriority = i_nExpandPrio;
        rEle.m_aMinSize = i_rMinSize;
        rEle.m_nX = i_nX;
        rEle.m_nY = i_nY;
        nIndex = it->second;
    }
    return nIndex;
}

void MatrixArranger::remove( Window* i_pWindow )
{
    if( i_pWindow )
    {
        for( std::vector< MatrixElement >::iterator it = m_aElements.begin();
            it != m_aElements.end(); ++it )
        {
            if( it->m_pElement == i_pWindow )
            {
                m_aMatrixMap.erase( getMap( it->m_nX, it->m_nY ) );
                m_aElements.erase( it );
                return;
            }
        }
    }
}

size_t MatrixArranger::addChild( boost::shared_ptr<WindowArranger> const &i_pChild, sal_uInt32 i_nX, sal_uInt32 i_nY, sal_Int32 i_nExpandPrio )
{
    sal_uInt64 nMapValue = getMap( i_nX, i_nY );
    std::map< sal_uInt64, size_t >::const_iterator it = m_aMatrixMap.find( nMapValue );
    size_t nIndex = 0;
    if( it == m_aMatrixMap.end() )
    {
        m_aMatrixMap[ nMapValue ] = nIndex = m_aElements.size();
        m_aElements.push_back( MatrixElement( NULL, i_nX, i_nY, i_pChild, i_nExpandPrio ) );
    }
    else
    {
        MatrixElement& rEle( m_aElements[ it->second ] );
        rEle.m_pElement = 0;
        rEle.m_pChild = i_pChild;
        rEle.m_nExpandPriority = i_nExpandPrio;
        rEle.m_nX = i_nX;
        rEle.m_nY = i_nY;
        nIndex = it->second;
    }
    return nIndex;
}

void MatrixArranger::remove( boost::shared_ptr<WindowArranger> const &i_pChild )
{
    if( i_pChild )
    {
        for( std::vector< MatrixElement >::iterator it = m_aElements.begin();
            it != m_aElements.end(); ++it )
        {
            if( it->m_pChild == i_pChild )
            {
                m_aMatrixMap.erase( getMap( it->m_nX, it->m_nY ) );
                m_aElements.erase( it );
                return;
            }
        }
    }
}

