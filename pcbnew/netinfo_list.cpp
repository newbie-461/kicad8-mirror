/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <board.h>
#include <board_commit.h>
#include <footprint.h>
#include <macros.h>
#include <pad.h>
#include <pcb_shape.h>
#include <pcb_track.h>
#include <zone.h>
#include <netinfo.h>
#include <wx/log.h>


// Constructor and destructor
NETINFO_LIST::NETINFO_LIST( BOARD* aParent ) :
    m_parent( aParent ),
    m_newNetCode( 0 )
{
    // Make sure that the unconnected net has number 0
    AppendNet( new NETINFO_ITEM( aParent, wxEmptyString, 0 ) );
}


NETINFO_LIST::~NETINFO_LIST()
{
    clear();
}


void NETINFO_LIST::clear()
{
    NETNAMES_MAP::iterator it, itEnd;

    for( it = m_netNames.begin(), itEnd = m_netNames.end(); it != itEnd; ++it )
        delete it->second;

    m_netNames.clear();
    m_netCodes.clear();
    m_newNetCode = 0;
}


NETINFO_ITEM* NETINFO_LIST::GetNetItem( int aNetCode ) const
{
    NETCODES_MAP::const_iterator result = m_netCodes.find( aNetCode );

    if( result != m_netCodes.end() )
        return (*result).second;

    return nullptr;
}


NETINFO_ITEM* NETINFO_LIST::GetNetItem( const wxString& aNetName ) const
{
    NETNAMES_MAP::const_iterator result = m_netNames.find( aNetName );

    if( result != m_netNames.end() )
        return (*result).second;

    return nullptr;
}


void NETINFO_LIST::RemoveNet( NETINFO_ITEM* aNet )
{
    bool removed = false;

    for( NETCODES_MAP::iterator i = m_netCodes.begin(); i != m_netCodes.end(); ++i )
    {
        if ( i->second == aNet )
        {
            removed = true;
            m_netCodes.erase(i);
            break;
        }
    }

    for( NETNAMES_MAP::iterator i = m_netNames.begin(); i != m_netNames.end(); ++i )
    {
        if ( i->second == aNet )
        {
            wxASSERT_MSG( removed, wxT( "NETINFO_LIST::RemoveNet: target net found in m_netNames "
                                        "but not m_netCodes!" ) );
            m_netNames.erase(i);
            break;
        }
    }

    if( removed )
    {
        m_newNetCode = std::min( m_newNetCode, aNet->m_netCode - 1 );
        m_DisplayNetnamesDirty = true;
    }
}


void NETINFO_LIST::RemoveUnusedNets( BOARD_COMMIT* aCommit )
{
    NETCODES_MAP               existingNets = m_netCodes;
    std::vector<NETINFO_ITEM*> unusedNets;

    m_netCodes.clear();
    m_netNames.clear();

    for( const auto& [ netCode, netInfo ] : existingNets )
    {
        if( netInfo->IsCurrent() )
        {
            m_netNames.insert( std::make_pair( netInfo->GetNetname(), netInfo ) );
            m_netCodes.insert( std::make_pair( netCode, netInfo ) );
        }
        else
        {
            m_DisplayNetnamesDirty = true;

            if( aCommit )
                aCommit->Removed( netInfo );
        }
    }
}


void NETINFO_LIST::AppendNet( NETINFO_ITEM* aNewElement )
{
    // if there is a net with such name then just assign the correct number
    NETINFO_ITEM* sameName = GetNetItem( aNewElement->GetNetname() );

    if( sameName != nullptr )
    {
        aNewElement->m_netCode = sameName->GetNetCode();

        return;
    }
    else if( aNewElement->m_netCode != (int) m_netCodes.size() || aNewElement->m_netCode < 0 )
    {
        // be sure that net codes are consecutive
        // negative net code means that it has to be auto assigned
        aNewElement->m_netCode = getFreeNetCode();
    }

    // net names & codes are supposed to be unique
    assert( GetNetItem( aNewElement->GetNetname() ) == nullptr );
    assert( GetNetItem( aNewElement->GetNetCode() ) == nullptr );

    // add an entry for fast look up by a net name using a map
    m_netNames.insert( std::make_pair( aNewElement->GetNetname(), aNewElement ) );
    m_netCodes.insert( std::make_pair( aNewElement->GetNetCode(), aNewElement ) );

    m_DisplayNetnamesDirty = true;
}


void NETINFO_LIST::buildListOfNets()
{
    // Restore the initial state of NETINFO_ITEMs
    for( NETINFO_ITEM* net : *this )
        net->Clear();

    m_parent->SynchronizeNetsAndNetClasses( false );
    m_parent->SetAreasNetCodesFromNetNames();
}


void NETINFO_LIST::RebuildDisplayNetnames() const
{
    std::map<wxString, int> shortNames;

    for( NETINFO_ITEM* net : *this )
        shortNames[net->m_shortNetname]++;

    for( NETINFO_ITEM* net : *this )
    {
        if( shortNames[net->m_shortNetname] == 1 )
            net->m_displayNetname = UnescapeString( net->m_shortNetname );
        else
            net->m_displayNetname = UnescapeString( net->m_netname );
    }

    m_DisplayNetnamesDirty = false;
}


#if defined(DEBUG)
void NETINFO_LIST::Show() const
{
    int i = 0;
    NETNAMES_MAP::const_iterator it, itEnd;

    for( it = m_netNames.begin(), itEnd = m_netNames.end(); it != itEnd; ++it )
    {
        wxLogDebug( wxT( "[%d]: netcode:%d  netname:<%s>\n" ),
                    i++,
                    it->second->GetNetCode(),
                    TO_UTF8( it->second->GetNetname() ) );
    }
}
#endif


int NETINFO_LIST::getFreeNetCode()
{
    do
    {
        if( m_newNetCode < 0 )
            m_newNetCode = 0;
    } while( m_netCodes.count( ++m_newNetCode ) != 0 );

    return m_newNetCode;
}


int NETINFO_MAPPING::Translate( int aNetCode ) const
{
    std::map<int, int>::const_iterator value = m_netMapping.find( aNetCode );

    if( value != m_netMapping.end() )
        return value->second;

    // There was no entry for the given net code
    return aNetCode;
}


void NETINFO_MAPPING::Update()
{
    // Collect all the used nets
    std::set<int> nets;

    // Be sure that the unconnected gets 0 and is mapped as 0
    nets.insert( 0 );

    // Zones
    for( ZONE* zone : m_board->Zones() )
        nets.insert( zone->GetNetCode() );

    // Tracks
    for( PCB_TRACK* track : m_board->Tracks() )
        nets.insert( track->GetNetCode() );

    for( BOARD_ITEM* item : m_board->Drawings() )
    {
        if( item->Type() != PCB_SHAPE_T )
            continue;

        PCB_SHAPE* shape = static_cast<PCB_SHAPE*>( item );

        if( shape->GetNetCode() > 0 )
            nets.insert( shape->GetNetCode() );
    }

    // footprints/pads
    for( FOOTPRINT* footprint : m_board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
            nets.insert( pad->GetNetCode() );
    }

    // Prepare the new mapping
    m_netMapping.clear();

    // Now the nets variable stores all the used net codes (not only for pads) and we are ready to
    // assign new consecutive net numbers
    int newNetCode = 0;

    for( auto net : nets )
        m_netMapping[net] = newNetCode++;
}


NETINFO_ITEM* NETINFO_MAPPING::iterator::operator*() const
{
    return m_mapping->m_board->FindNet( m_iterator->first );
}


NETINFO_ITEM* NETINFO_MAPPING::iterator::operator->() const
{
    return m_mapping->m_board->FindNet( m_iterator->first );
}


const int NETINFO_LIST::UNCONNECTED = 0;
const int NETINFO_LIST::ORPHANED = -1;

