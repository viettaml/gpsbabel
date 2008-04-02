/* 

	Reader for "OpenStreetMap" data files (.xml)

	Copyright (C) 2008 Olaf Klein, o.b.klein@gpsbabel.org

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111 USA

*/

#include "defs.h"
#include "avltree.h"
#include "xmlgeneric.h"

static arglist_t osm_args[] = 
{
	ARG_TERMINATOR
};

#define MYNAME "osm"

#if ! HAVE_LIBEXPAT
void
osm_rd_init(const char *fname)
{
	fatal(MYNAME ": This build excluded \" MYNAME \" support because expat was not installed.\n");
}

void
osm_read(void)
{
}

#else

static waypoint *wpt;
static route_head *rte;
static int wpt_loaded, rte_loaded;

static avltree_t *waypoints;	/* AVL tree */
static avltree_t *keys = NULL;
static avltree_t *values = NULL;

static xg_callback	osm_node, osm_node_tag, osm_node_end;
static xg_callback	osm_way, osm_way_nd, osm_way_tag, osm_way_end;

static 
xg_tag_mapping osm_map[] = {
	{ osm_node,	cb_start,	"/osm/node" },
	{ osm_node_tag,	cb_start,	"/osm/node/tag" },
	{ osm_node_end,	cb_end,		"/osm/node" },
	{ osm_way,	cb_start,	"/osm/way" },
	{ osm_way_nd,	cb_start,	"/osm/way/nd" },
	{ osm_way_tag,	cb_start,	"/osm/way/tag" },
	{ osm_way_end,	cb_end,		"/osm/way" },
	{ NULL,		0,		NULL }
};

static char *osm_features[] = {
	"- dummy -",	/*  0 */
	"aeroway",	/*  1 */
	"amenity",	/*  2 */
	"building",	/*  3 */
	"cycleway",	/*  4 */
	"railway",	/*  5 */
	"highway",	/*  6 */
	"historic",	/*  7 */
	"landuse",	/*  8 */
	"leisure",	/*  9 */
	"man_made",	/* 10 */
	"military",	/* 11 */
	"natural",	/* 12 */
	"place",	/* 13 */
	"power",	/* 14 */
	"shop",		/* 15 */
	"sport",	/* 16 */
	"tourism",	/* 17 */
	"waterway",	/* 18 */
	"aerialway",	/* 19 */
	NULL
};

typedef struct osm_icon_mapping_s {
	const char key;
	const char *value;
	const char *icon;
} osm_icon_mapping_t;


/* based on <http://wiki.openstreetmap.org/index.php/Map_Features> */

static osm_icon_mapping_t osm_icon_mappings[] = {

	/* cycleway ...*/

	/* highway ...*/

//	{ 6, "mini_roundabout",		"?" },
//	{ 6, "stop",			"?" },
//	{ 6, "traffic_signals",		"?" },
//	{ 6, "crossing",		"?" },
//	{ 6, "gate",			"?" },
//	{ 6, "stile",			"?" },
//	{ 6, "cattle_grid",		"?" },
//	{ 6, "toll_booth",		"?" },
//	{ 6, "incline",			"?" },
//	{ 6, "incline_steep",		"?" },
//	{ 6, "viaduct",			"?" },
//	{ 6, "motorway_junction",	"?" },
//	{ 6, "services",		"?" },
//	{ 6, "ford",			"?" },
//	{ 6, "bus_stop",		"?" },
//	{ 6, "turning_circle",		"?" },
//	{ 6, "User Defined",		"?" },

	/* waterway ... */

	{ 18, "dock",			"Dock" },
//	{ 18, "lock_gate",		"?" },
//	{ 18, "turning_point",		"?" },
//	{ 18, "aqueduct",		"?" },
//	{ 18, "boatyard",		"?" },
//	{ 18, "water_point",		"?" },
//	{ 18, "waste_disposal",		"?" },
//	{ 18, "mooring",		"?" },
//	{ 18, "weir",			"?" },
//	{ 18, "User Defined",		"?" },

	/* railway ... */

//	{ 5, "station",			"?" },
//	{ 5, "halt",			"?" },
//	{ 5, "tram_stop",		"?" },
//	{ 5, "viaduct",			"?" },
	{ 5, "crossing",		"Crossing" },
//	{ 5, "level_crossing",		"?" },
//	{ 5, "subway_entrance",		"?" },
//	{ 5, "turntable",		"?" },
//	{ 5, "User Defined",		"?" },

	/* aeroway ... */

	{ 1, "aerodrome",		"Airport" },
	{ 1, "terminal",		"Airport" },
	{ 1, "helipad",			"Heliport" },
//	{ 1, "User Defined",		"?" },

	/* aerialway ... */

//	{ 19, "User Defined",		"?" },

	/* power ... */

//	{ 14, "tower",			"?" },
//	{ 14, "sub_station",		"?" },
//	{ 14, "generator",		"?" },

	/* man_made ... */

//	{ 10, "works",			"?" },
//	{ 10, "beacon",			"?" },
//	{ 10, "survey_point",		"?" },
//	{ 10, "power_wind",		"?" },
//	{ 10, "power_hydro",		"?" },
//	{ 10, "power_fossil",		"?" },
//	{ 10, "power_nuclear",		"?" },
//	{ 10, "tower",			"?" },
//	{ 10, "water_tower",		"?" },
//	{ 10, "gasometer",		"?" },
//	{ 10, "reservoir_covered",	"?" },
//	{ 10, "lighthouse",		"?" },
//	{ 10, "windmill",		"?" },
//	{ 10, "wastewater_plant",	"?" },
//	{ 10, "crane",			"?" },
//	{ 10, "User Defined",		"?" },

	/* building ... */

	{ 3, "yes",			"Building" },
//	{ 3, "User Defined",		"?" },

	/* leisure ... */

//	{ 9, "sports_centre",		"?" },
	{ 9, "golf_course",		"Golf Course" },
	{ 9, "stadium",			"Stadium" },
//	{ 9, "track",			"?" },
//	{ 9, "pitch",			"?" },
//	{ 9, "water_park",		"?" },
	{ 9, "marina",			"Marina" },
//	{ 9, "slipway",			"?" },
	{ 9, "fishing",			"Fishing Area" },
//	{ 9, "nature_reserve",		"?" },
	{ 9, "park",			"Park" },
//	{ 9, "playground",		"?" },
//	{ 9, "garden",			"?" },
//	{ 9, "common",			"?" },
//	{ 9, "User Defined",		"?" },

	/* amenity ... */

	{ 2, "pub",			"Bar" },
//	{ 2, "biergarten",		"?" },
	{ 2, "nightclub",		"Bar" },
//	{ 2, "cafe",			"?" },
	{ 2, "restaurant",		"Restaurant" },
	{ 2, "fast_food",		"Fast Food" },
	{ 2, "parking",			"Parking Area" },
//	{ 2, "bicycle_parking",		"?" },
//	{ 2, "bicycle_rental",		"?" },
	{ 2, "car_rental",		"Car Rental" },
//	{ 2, "car_sharing",		"?" },
//	{ 2, "taxi",			"?" },
	{ 2, "fuel",			"Gas Station" },
	{ 2, "telephone",		"Telephone" },
	{ 2, "toilets",			"Restroom" },
//	{ 2, "recycling",		"?" },
//	{ 2, "public_building",		"?" },
	{ 2, "townhall",		"City Hall" },
//	{ 2, "place_of_worship",	"?" },
//	{ 2, "grave_yard",		"?" },
	{ 2, "post_office",		"Post Office" },
//	{ 2, "post_box",		"?" },
	{ 2, "school",			"School" },
//	{ 2, "university",		"?" },
//	{ 2, "college",			"?" },
	{ 2, "pharmacy",		"Pharmacy" },
	{ 2, "hospital",		"Medical Facility" },
//	{ 2, "library",			"?" },
	{ 2, "police",			"Police Station" },
//	{ 2, "fire_station",		"?" },
//	{ 2, "bus_station",		"?" },
//	{ 2, "theatre",			"?" },
//	{ 2, "cinema",			"?" },
//	{ 2, "arts_centre",		"?" },
//	{ 2, "courthouse",		"?" },
//	{ 2, "prison",			"?" },
	{ 2, "bank",			"Bank" },
//	{ 2, "bureau_de_change",	"?" },
//	{ 2, "atm",			"?" },
//	{ 2, "fountain",		"?" },
//	{ 2, "User Defined",		"?" },

	/* shop ... */

//	{ 15, "supermarket",		"?" },
	{ 15, "convenience",		"Convenience Store" },
//	{ 15, "butcher",		"?" },
//	{ 15, "bicycle",		"?" },
//	{ 15, "doityourself",		"?" },
//	{ 15, "dry_cleaning",		"?" },
//	{ 15, "laundry",		"?" },
//	{ 15, "outdoor",		"?" },
//	{ 15, "kiosk",			"?" },
//	{ 15, "User Defined",		"?" },

	/* tourism ... */

	{ 17, "information",		"Information" },
	{ 17, "hotel",			"Hotel" },
	{ 17, "motel",			"Lodging" },
	{ 17, "guest_house",		"Lodging" },
	{ 17, "hostel",			"Lodging" },
	{ 17, "camp_site",		"Campground" },
	{ 17, "caravan_site",		"RV Park" },
	{ 17, "picnic_site",		"Picnic Area" },
	{ 17, "viewpoint",		"Scenic Area" },
//	{ 17, "theme_park",		"?" },
//	{ 17, "attraction",		"?" },
	{ 17, "zoo",			"Zoo" },
//	{ 17, "artwork",		"?" },
	{ 17, "museum",			"Museum" },
//	{ 17, "User Defined",		"?" },

	/* historic ... */

//	{ 7, "castle",			"?" },
//	{ 7, "monument",		"?" },
//	{ 7, "memorial",		"?" },
//	{ 7, "archaeological_site",	"?" },
//	{ 7, "ruins",			"?" },
//	{ 7, "battlefield",		"?" },
//	{ 7, "User Defined",		"?" },

	/* landuse ... */

//	{ 8, "farm",			"?" },
//	{ 8, "quarry",			"?" },
//	{ 8, "landfill",		"?" },
//	{ 8, "basin",			"?" },
//	{ 8, "reservoir",		"?" },
	{ 8, "forest",			"Forest" },
//	{ 8, "allotments",		"?" },
//	{ 8, "residential",		"?" },
//	{ 8, "retail",			"?" },
//	{ 8, "commercial",		"?" },
//	{ 8, "industrial",		"?" },
//	{ 8, "brownfield",		"?" },
//	{ 8, "greenfield",		"?" },
//	{ 8, "railway",			"?" },
//	{ 8, "construction",		"?" },
	{ 8, "military",		"Military" },
	{ 8, "cemetery",		"Cemetery" },
//	{ 8, "village_green",		"?" },
//	{ 8, "recreation_ground",	"?" },
//	{ 8, "User Defined",		"?" },

	/* military ... */

//	{ 11, "airfield",		"?" },
//	{ 11, "bunker",			"?" },
//	{ 11, "barracks",		"?" },
//	{ 11, "danger_area",		"?" },
//	{ 11, "range",			"?" },
//	{ 11, "naval_base",		"?" },
//	{ 11, "User Defined",		"?" },

	/* natural ... */

//	{ 12, "spring",			"?" },
//	{ 12, "peak",			"?" },
//	{ 12, "glacier",		"?" },
//	{ 12, "volcano",		"?" },
//	{ 12, "cliff",			"?" },
//	{ 12, "scree",			"?" },
//	{ 12, "scrub",			"?" },
//	{ 12, "fell",			"?" },
//	{ 12, "heath",			"?" },
//	{ 12, "wood",			"?" },
//	{ 12, "marsh",			"?" },
//	{ 12, "water",			"?" },
//	{ 12, "coastline",		"?" },
//	{ 12, "mud",			"?" },
	{ 12, "beach",			"Beach" },
//	{ 12, "bay",			"?" },
//	{ 12, "land",			"?" },
//	{ 12, "cave_entrance",		"?" },
//	{ 12, "User Defined",		"?" },

	/* sport ... */

//	{ 16, "10pin",			"?" },
//	{ 16, "athletics",		"?" },
//	{ 16, "australian_football",	"?" },
//	{ 16, "baseball",		"?" },
//	{ 16, "basketball",		"?" },
//	{ 16, "boules",			"?" },
//	{ 16, "bowls",			"?" },
//	{ 16, "climbing",		"?" },
//	{ 16, "cricket",		"?" },
//	{ 16, "cricket_nets",		"?" },
//	{ 16, "croquet",		"?" },
//	{ 16, "cycling",		"?" },
//	{ 16, "dog_racing",		"?" },
//	{ 16, "equestrian",		"?" },
//	{ 16, "football",		"?" },
//	{ 16, "golf",			"?" },
//	{ 16, "gymnastics",		"?" },
//	{ 16, "hockey",			"?" },
//	{ 16, "horse_racing",		"?" },
//	{ 16, "motor",			"?" },
//	{ 16, "multi",			"?" },
//	{ 16, "pelota",			"?" },
//	{ 16, "racquet",		"?" },
//	{ 16, "rugby",			"?" },
//	{ 16, "skating",		"?" },
//	{ 16, "skateboard",		"?" },
//	{ 16, "soccer",			"?" },
	{ 16, "swimming",		"Swimming Area" },
	{ 16, "skiing",			"Skiing Area" },
//	{ 16, "table_tennis",		"?" },
//	{ 16, "tennis",			"?" },
//	{ 16, "orienteering",		"?" },
//	{ 16, "User Defined",		"?" },

	/* place ... */

//	{ 13, "continent",		"?" },
//	{ 13, "country",		"?" },
//	{ 13, "state",			"?" },
//	{ 13, "region",			"?" },
//	{ 13, "county",			"?" },
	{ 13, "city",			"City (Large)" },
	{ 13, "town",			"City (Medium)" },
	{ 13, "village",		"City (Small)" },
//	{ 13, "hamlet",			"?" },
//	{ 13, "suburb",			"?" },
//	{ 13, "locality",		"?" },
//	{ 13, "island",			"?" },
//	{ 13, "User Defined",		"?" },

	{ -1, NULL, NULL }
};


static void
osm_features_init(void)
{
	/* here we take a union because of warnings
	   "cast to pointer from integer of different size" 
	   on 64-bit systems */
	union {
		const void *p;
		int i;
	} x;

	keys = avltree_init(0, MYNAME);
	values = avltree_init(0, MYNAME);
	
	x.p = NULL;
	
	/* the first of osm_features is a place holder */
	for (x.i = 1; osm_features[x.i]; x.i++)
		avltree_insert(keys, osm_features[x.i], x.p);
	
	for (x.i = 0; osm_icon_mappings[x.i].value; x.i++) {
		char buff[128];

		buff[0] = osm_icon_mappings[x.i].key;
		strncpy(&buff[1], osm_icon_mappings[x.i].value, sizeof(buff) - 1);
		avltree_insert(values, buff, (const void *)&osm_icon_mappings[x.i]);
	}
}


static char
osm_feature_ikey(const char *key)
{
	int result;
	union {
		const void *p;
		int i;
	} x;
	
	if (avltree_find(keys, key, &x.p))
		result = x.i;
	else
		result = -1;

	return result;
}


static char *
osm_feature_symbol(const char ikey, const char *value)
{
	char *result;
	char buff[128];
	osm_icon_mapping_t *data;

	buff[0] = ikey;
	strncpy(&buff[1], value, sizeof(buff) - 1);

	if (avltree_find(values, buff, (void *)&data))
		result = xstrdup(data->icon);
	else
		xasprintf(&result, "%s:%s", osm_features[(int)ikey], value);

	return result;
}


static char *
osm_strip_html(const char *str)
{
	utf_string utf;
	utf.is_html = 1;
	utf.utfstring = (char *)str;

	return strip_html(&utf);	// util.c
}


static void 
osm_node_end(const char *args, const char **unused)
{
	if (wpt) {
		if (wpt->wpt_flags.fmt_use)
			waypt_add(wpt);
		else
			waypt_free(wpt);
		wpt = NULL;
	}
}


static void 
osm_node(const char *args, const char **attrv)
{
	const char **avp = &attrv[0];

	wpt = waypt_new();

        while (*avp) {
		if (strcmp(avp[0], "id") == 0) {
			xasprintf(&wpt->description, "osm-id %s", avp[1]);
			if (! avltree_insert(waypoints, avp[1], (void *)wpt))
				warning(MYNAME ": Duplicate osm-id %s!\n", avp[1]);
			else
				wpt->wpt_flags.fmt_use = 1;
		}
		else if (strcmp(avp[0], "user") == 0) ;
		else if (strcmp(avp[0], "lat") == 0)
			wpt->latitude = atof(avp[1]);
		else if (strcmp(avp[0], "lon") == 0)
			wpt->longitude = atof(avp[1]);
		else if (strcmp(avp[0], "timestamp") == 0)
			wpt->creation_time = xml_parse_time(avp[1], &wpt->microseconds);

		avp += 2;
	}
}


static void 
osm_node_tag(const char *args, const char **attrv)
{
	const char **avp = &attrv[0];
	const char *key = "", *value = "";
	char *str;
	char ikey;
	
        while (*avp) {
		if (strcmp(avp[0], "k") == 0)
			key = avp[1];
		else if (strcmp(avp[0], "v") == 0)
			value = avp[1];
		avp+=2;
	}
	
	str = osm_strip_html(value);
	
	if (strcmp(key, "name") == 0) {
		if (! wpt->shortname)
			wpt->shortname = xstrdup(str);
	}
	else if (strcmp(key, "name:en") == 0) {
		if (wpt->shortname)
			xfree(wpt->shortname);
		wpt->shortname = xstrdup(str);
	}
	else if ((ikey = osm_feature_ikey(key)) >= 0) {
		wpt->icon_descr = osm_feature_symbol(ikey, value);
		wpt->wpt_flags.icon_descr_is_dynamic = 1;
	}
	else if (strcmp(key, "note") == 0) {
		if (wpt->notes) {
			char *tmp;
			xasprintf(&tmp, "%s; %s", wpt->notes, str);
			xfree(wpt->notes);
			wpt->notes = tmp;
		}
		else
			wpt->notes = xstrdup(str);
	}
	
	xfree(str);
}


static void 
osm_way(const char *args, const char **attrv)
{
	const char **avp = &attrv[0];

	rte = route_head_alloc();

        while (*avp) {
		if (strcmp(avp[0], "id") == 0) {
			xasprintf(&rte->rte_desc, "osm-id %s", avp[1]);
		}
		avp += 2;
	}
}

static void 
osm_way_nd(const char *args, const char **attrv)
{
	const char **avp = &attrv[0];

        while (*avp) {
		if (strcmp(avp[0], "ref") == 0) {
			waypoint *tmp;
			if (avltree_find(waypoints, avp[1], (void *)&tmp)) {
				tmp = waypt_dupe(tmp);
				route_add_wpt(rte, tmp);
			}
			else
				warning(MYNAME ": Way reference id \"%s\" wasn't listed under nodes!\n", avp[1]);
		}
		avp += 2;
	}
}

static void 
osm_way_tag(const char *args, const char **attrv)
{
	const char **avp = &attrv[0];
	const char *key = "", *value = "";
	char *str;
	
        while (*avp) {
		if (strcmp(avp[0], "k") == 0)
			key = avp[1];
		else if (strcmp(avp[0], "v") == 0)
			value = avp[1];
		avp += 2;
	}
	
	str = osm_strip_html(value);
	
	if (strcmp(key, "name") == 0) {
		if (! rte->rte_name)
			rte->rte_name = xstrdup(str);
	}
	else if (strcmp(key, "name:en") == 0) {
		if (rte->rte_name)
			xfree(rte->rte_name);
		rte->rte_name = xstrdup(str);
	}
	
	xfree(str);
}

static void 
osm_way_end(const char *args, const char **unused)
{
	if (rte) {
		route_add_head(rte);
		rte = NULL;
	}
}

static void 
osm_rd_init(const char *fname)
{
	wpt = NULL;
	rte = NULL;
	wpt_loaded = 0;
	rte_loaded = 0;

	waypoints = avltree_init(0, MYNAME);
	if (! keys)
		osm_features_init();

	xml_init(fname, osm_map, NULL);
}

static void 
osm_read(void)
{
	xml_read();
	avltree_done(waypoints);
}

#endif

static void 
osm_rd_deinit(void)
{
	xml_deinit();
}

static void
osm_exit(void)
{
	if (keys)
		avltree_done(keys);
	if (values)
		avltree_done(values);
}


ff_vecs_t osm_vecs = {
	ff_type_file,
	{
	  ff_cap_read,	/* waypoints */
	  ff_cap_none, 	/* tracks */
	  ff_cap_read	/* routes */
	},
	osm_rd_init,	
	NULL,	
	osm_rd_deinit,
	NULL,
	osm_read,
	NULL,
	osm_exit,
	osm_args,
	CET_CHARSET_UTF8, 0
};