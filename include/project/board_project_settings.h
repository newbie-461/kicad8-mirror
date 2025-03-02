/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Jon Evans <jon@craftyjon.com>
 * Copyright (C) 2020-2023 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KICAD_BOARD_PROJECT_SETTINGS_H
#define KICAD_BOARD_PROJECT_SETTINGS_H

#include <layer_ids.h>
#include <settings/parameters.h>

// Can be removed by refactoring PARAM_LAYER_PRESET
#include <nlohmann/json.hpp>
#include <math/box2.h>
#include <glm/glm.hpp>

/**
 * This file contains data structures that are saved in the project file or project local settings
 * file that are specific to PcbNew.  This is done so that these structures are available in common.
 */


/**
 * Selection filtering that applies all the time (not the "filter selection" dialog that modifies
 * the current selection)
 */
struct SELECTION_FILTER_OPTIONS
{
    bool lockedItems;   ///< Allow selecting locked items
    bool footprints;    ///< Allow selecting entire footprints
    bool text;          ///< Text (free or attached to a footprint)
    bool tracks;        ///< Copper tracks
    bool vias;          ///< Vias (all types>
    bool pads;          ///< Footprint pads
    bool graphics;      ///< Graphic lines, shapes, polygons
    bool zones;         ///< Copper zones
    bool keepouts;      ///< Keepout zones
    bool dimensions;    ///< Dimension items
    bool otherItems;    ///< Anything not fitting one of the above categories

    SELECTION_FILTER_OPTIONS()
    {
        lockedItems = true;
        footprints  = true;
        text        = true;
        tracks      = true;
        vias        = true;
        pads        = true;
        graphics    = true;
        zones       = true;
        keepouts    = true;
        dimensions  = true;
        otherItems  = true;
    }

    /**
     * @return true if any of the item types are enabled (excluding "locked items" which is special)
     */
    bool Any()
    {
        return ( footprints || text || tracks || vias || pads || graphics || zones
                 || keepouts || dimensions || otherItems );
    }

    /**
     * @return true if all the item types are enabled (excluding "locked items" which is special)
     */
    bool All()
    {
        return ( footprints && text && tracks && vias && pads && graphics && zones
                 && keepouts && dimensions && otherItems );
    }
};

/**
 * Determine how inactive layers should be displayed.
 */
enum class HIGH_CONTRAST_MODE
{
    NORMAL = 0,     ///< Inactive layers are shown normally (no high-contrast mode)
    DIMMED,         ///< Inactive layers are dimmed (old high-contrast mode)
    HIDDEN          ///< Inactive layers are hidden
};

///< Determine how zones should be displayed.
enum class ZONE_DISPLAY_MODE
{
    SHOW_FILLED,
    SHOW_ZONE_OUTLINE,

    // Debug modes

    SHOW_FRACTURE_BORDERS,
    SHOW_TRIANGULATION
};

///< Determine how net color overrides should be applied.
enum class NET_COLOR_MODE
{
    OFF,        ///< Net (and netclass) colors are not shown
    RATSNEST,   ///< Net/netclass colors are shown on ratsnest lines only
    ALL         ///< Net/netclass colors are shown on all net copper
};

///< Determine how ratsnest lines are drawn.
enum class RATSNEST_MODE
{
    ALL,        ///< Ratsnest lines are drawn to items on all layers (default)
    VISIBLE     ///< Ratsnest lines are drawn to items on visible layers only
};

///< BOM Data choices for IPC2581 export
struct IP2581_BOM
{
    wxString mfg;       ///< Manufacturer name column
    wxString MPN;     ///< Manufacturer part number column
    wxString dist;      ///< Distributor name column
    wxString distPN;    ///< Distributor part number column
    wxString id;        ///< Internal ID column
};

/**
 * A saved set of layers that are visible.
 */
struct LAYER_PRESET
{
    LAYER_PRESET( const wxString& aName = wxS( "" ) ) :
            name( aName ),
            layers( LSET::AllLayersMask() ),
            renderLayers( GAL_SET::DefaultVisible() ),
            flipBoard( false ),
            activeLayer( UNSELECTED_LAYER )
    {
        readOnly     = false;
    }

    LAYER_PRESET( const wxString& aName, const LSET& aVisibleLayers, bool aFlipBoard ) :
            name( aName ),
            layers( aVisibleLayers ),
            renderLayers( GAL_SET::DefaultVisible() ),
            flipBoard( aFlipBoard ),
            activeLayer( UNSELECTED_LAYER )
    {
        readOnly     = false;
    }

    LAYER_PRESET( const wxString& aName, const LSET& aVisibleLayers, const GAL_SET& aVisibleObjects,
                  PCB_LAYER_ID aActiveLayer, bool aFlipBoard ) :
            name( aName ),
            layers( aVisibleLayers ),
            renderLayers( aVisibleObjects ),
            flipBoard( aFlipBoard ),
            activeLayer( aActiveLayer )
    {
        readOnly  = false;
    }

    bool LayersMatch( const LAYER_PRESET& aOther )
    {
        return aOther.layers == layers && aOther.renderLayers == renderLayers;
    }

    wxString     name;          ///< A name for this layer set
    LSET         layers;        ///< Board layers that are visible
    GAL_SET      renderLayers;  ///< Render layers (e.g. object types) that are visible
    bool         flipBoard;     ///< True if the flip board is enabled
    PCB_LAYER_ID activeLayer;   ///< Optional layer to set active when this preset is loaded
    bool         readOnly;      ///< True if this is a read-only (built-in) preset
};


class PARAM_LAYER_PRESET : public PARAM_LAMBDA<nlohmann::json>
{
public:
    PARAM_LAYER_PRESET( const std::string& aPath, std::vector<LAYER_PRESET>* aPresetList );

private:
    nlohmann::json presetsToJson();

    void jsonToPresets( const nlohmann::json& aJson );

    std::vector<LAYER_PRESET>* m_presets;
};


struct VIEWPORT
{
    VIEWPORT( const wxString& aName = wxEmptyString ) :
            name( aName )
    { }

    VIEWPORT( const wxString& aName, const BOX2D& aRect ) :
            name( aName ),
            rect( aRect )
    { }

    wxString name;
    BOX2D    rect;
};


class PARAM_VIEWPORT : public PARAM_LAMBDA<nlohmann::json>
{
public:
    PARAM_VIEWPORT( const std::string& aPath, std::vector<VIEWPORT>* aViewportList );

private:
    nlohmann::json viewportsToJson();

    void jsonToViewports( const nlohmann::json& aJson );

    std::vector<VIEWPORT>* m_viewports;
};


struct VIEWPORT3D
{
    VIEWPORT3D( const wxString& aName = wxEmptyString ) :
            name( aName )
    { }

    VIEWPORT3D( const wxString& aName, glm::mat4 aViewMatrix ) :
            name( aName ),
            matrix( std::move( aViewMatrix ) )
    { }

    wxString  name;
    glm::mat4 matrix;
};


class PARAM_VIEWPORT3D : public PARAM_LAMBDA<nlohmann::json>
{
public:
    PARAM_VIEWPORT3D( const std::string& aPath, std::vector<VIEWPORT3D>* aViewportList );

private:
    nlohmann::json viewportsToJson();

    void jsonToViewports( const nlohmann::json & aJson );

    std::vector<VIEWPORT3D>* m_viewports;
};


#endif // KICAD_BOARD_PROJECT_SETTINGS_H
