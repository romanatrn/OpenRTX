#!/usr/bin/env python3

import json
import math
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT_PATH = ROOT / "openrtx" / "src" / "ui" / "common" / "gps_map_data.inc"

NS_BBOX = (-66.8, 43.2, -59.0, 47.2)

BOUNDARY_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_admin_1_states_provinces.geojson"
ROADS_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_roads.geojson"
RIVERS_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_rivers_lake_centerlines.geojson"
PLACES_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_populated_places.geojson"


def fetch_json(url):
    with urllib.request.urlopen(url, timeout=180) as resp:
        return json.load(resp)


def rdp(points, epsilon):
    if len(points) <= 2:
        return points[:]

    start = points[0]
    end = points[-1]
    max_dist = -1.0
    max_index = -1

    sx, sy = start
    ex, ey = end
    dx = ex - sx
    dy = ey - sy
    seg_len_sq = dx * dx + dy * dy

    for i in range(1, len(points) - 1):
        px, py = points[i]
        if seg_len_sq == 0.0:
            dist = math.hypot(px - sx, py - sy)
        else:
            t = ((px - sx) * dx + (py - sy) * dy) / seg_len_sq
            proj_x = sx + t * dx
            proj_y = sy + t * dy
            dist = math.hypot(px - proj_x, py - proj_y)
        if dist > max_dist:
            max_dist = dist
            max_index = i

    if max_dist > epsilon:
        left = rdp(points[: max_index + 1], epsilon)
        right = rdp(points[max_index:], epsilon)
        return left[:-1] + right

    return [start, end]


def point_in_bbox(pt, bbox=NS_BBOX):
    x, y = pt
    minx, miny, maxx, maxy = bbox
    return minx <= x <= maxx and miny <= y <= maxy


def any_point_in_bbox(points, bbox=NS_BBOX):
    return any(point_in_bbox(pt, bbox) for pt in points)


def iter_lines(geometry):
    if geometry["type"] == "LineString":
        yield geometry["coordinates"]
    elif geometry["type"] == "MultiLineString":
        for line in geometry["coordinates"]:
            yield line


def polygon_rings(feature):
    geom = feature["geometry"]
    if geom["type"] == "Polygon":
        return geom["coordinates"]
    if geom["type"] == "MultiPolygon":
        rings = []
        for poly in geom["coordinates"]:
            rings.extend(poly)
        return rings
    return []


def quantize(points):
    out = []
    for lon, lat in points:
        qlat = int(round(lat * 1000000.0))
        qlon = int(round(lon * 1000000.0))
        if not out or out[-1] != (qlat, qlon):
            out.append((qlat, qlon))
    return out


def sanitize_name(name):
    cleaned = []
    for ch in name.lower():
        if ch.isalnum():
            cleaned.append(ch)
        else:
            cleaned.append("_")
    return "gps_map_" + "".join(cleaned).strip("_")


def feature_min_zoom(kind, props):
    if kind == "coast":
        return "GPS_MAP_ZOOM_PROVINCE"
    if kind == "major_road":
        rank = int(props.get("scalerank", 8) or 8)
        return "GPS_MAP_ZOOM_PROVINCE" if rank <= 5 else "GPS_MAP_ZOOM_REGION"
    if kind == "secondary_road":
        rank = int(props.get("scalerank", 8) or 8)
        return "GPS_MAP_ZOOM_REGION" if rank <= 6 else "GPS_MAP_ZOOM_TOWN"
    if kind == "water":
        rank = int(props.get("scalerank", 8) or 8)
        return "GPS_MAP_ZOOM_REGION" if rank <= 5 else "GPS_MAP_ZOOM_TOWN"
    return "GPS_MAP_ZOOM_REGION"


def label_min_zoom(props):
    rank = int(props.get("LABELRANK", 7) or 7)
    return "GPS_MAP_ZOOM_PROVINCE" if rank <= 2 else "GPS_MAP_ZOOM_REGION"


def trim_label(name):
    repl = {
        "Bridgewater": "Bridgewtr",
        "New Glasgow": "NewGlsgw",
        "Port Hawkesbury": "P.Hawk",
    }
    return repl.get(name, name[:10])


def bbox_for(points):
    lats = [p[0] for p in points]
    lons = [p[1] for p in points]
    return min(lats), max(lats), min(lons), max(lons)


def load_boundary():
    data = fetch_json(BOUNDARY_URL)
    for feature in data["features"]:
        props = feature["properties"]
        if props.get("name_en") != "Nova Scotia":
            continue
        polygons = []
        for ring in polygon_rings(feature):
            simplified = rdp(ring, 0.01)
            q = quantize(simplified)
            if len(q) >= 4:
                if q[0] != q[-1]:
                    q.append(q[0])
                polygons.append(q)
        return polygons
    raise RuntimeError("Nova Scotia boundary not found")


def load_roads():
    data = fetch_json(ROADS_URL)
    features = []
    for feature in data["features"]:
        props = feature["properties"]
        road_type = props.get("type")
        if road_type not in ("Major Highway", "Secondary Highway"):
            continue
        for line in iter_lines(feature["geometry"]):
            if not any_point_in_bbox(line):
                continue
            simplified = rdp(line, 0.015 if road_type == "Major Highway" else 0.01)
            q = quantize(simplified)
            if len(q) < 2:
                continue
            kind = "major_road" if road_type == "Major Highway" else "secondary_road"
            features.append((kind, feature_min_zoom(kind, props), props, q))
    return features


def load_rivers():
    data = fetch_json(RIVERS_URL)
    features = []
    for feature in data["features"]:
        props = feature["properties"]
        for line in iter_lines(feature["geometry"]):
            if not any_point_in_bbox(line):
                continue
            simplified = rdp(line, 0.01)
            q = quantize(simplified)
            if len(q) < 2:
                continue
            features.append(("water", feature_min_zoom("water", props), props, q))
    return features


def load_labels():
    data = fetch_json(PLACES_URL)
    labels = []
    for feature in data["features"]:
        props = feature["properties"]
        if props.get("ADM1NAME") != "Nova Scotia":
            continue
        name = props.get("NAME") or props.get("NAMEASCII")
        if not name:
            continue
        lon, lat = feature["geometry"]["coordinates"]
        if not point_in_bbox((lon, lat)):
            continue
        labels.append((int(props.get("LABELRANK", 7) or 7), trim_label(name), int(round(lat * 1000000.0)), int(round(lon * 1000000.0)), label_min_zoom(props)))
    labels.sort()
    seen = set()
    out = []
    for _, name, lat, lon, zoom in labels:
        if name in seen:
            continue
        seen.add(name)
        out.append((name, lat, lon, zoom))
        if len(out) >= 10:
            break
    return out


def emit():
    polygons = load_boundary()
    road_features = load_roads()
    river_features = load_rivers()
    labels = load_labels()

    lines = []
    lines.append("/* Auto-generated by scripts/generate_gps_map_data.py */")
    lines.append("")

    polygon_names = []
    land_entries = []
    feature_entries = []

    for idx, poly in enumerate(polygons):
        name = f"gps_map_land_{idx}"
        polygon_names.append(name)
        lines.append(f"static const gps_map_point_t {name}[] = {{")
        for lat, lon in poly:
            lines.append(f"    {{{lat}, {lon}}},")
        lines.append("};")
        lines.append("")
        land_entries.append(f"    {{{name}, ARRAY_SIZE({name})}},")
        min_lat, max_lat, min_lon, max_lon = bbox_for(poly)
        feature_entries.append(
            f"    {{{name}, ARRAY_SIZE({name}), {min_lat}, {max_lat}, {min_lon}, {max_lon}, GPS_MAP_ZOOM_PROVINCE, GPS_MAP_FEATURE_COAST}},"
        )

    def add_line_feature(prefix, index, pts, zoom, kind_enum):
        name = f"{prefix}_{index}"
        lines.append(f"static const gps_map_point_t {name}[] = {{")
        for lat, lon in pts:
            lines.append(f"    {{{lat}, {lon}}},")
        lines.append("};")
        lines.append("")
        min_lat, max_lat, min_lon, max_lon = bbox_for(pts)
        feature_entries.append(
            f"    {{{name}, ARRAY_SIZE({name}), {min_lat}, {max_lat}, {min_lon}, {max_lon}, {zoom}, {kind_enum}}},"
        )

    for idx, (_, zoom, _, pts) in enumerate(road_features):
        kind_enum = "GPS_MAP_FEATURE_MAJOR_ROAD" if road_features[idx][0] == "major_road" else "GPS_MAP_FEATURE_SECONDARY_ROAD"
        add_line_feature("gps_map_road", idx, pts, zoom, kind_enum)

    for idx, (_, zoom, _, pts) in enumerate(river_features):
        add_line_feature("gps_map_water", idx, pts, zoom, "GPS_MAP_FEATURE_WATER")

    lines.append("typedef struct")
    lines.append("{")
    lines.append("    const gps_map_point_t *points;")
    lines.append("    uint16_t count;")
    lines.append("} gps_map_polygon_ref_t;")
    lines.append("")
    lines.append("static const gps_map_polygon_ref_t map_land_polygons[] = {")
    lines.extend(land_entries)
    lines.append("};")
    lines.append("")
    lines.append("static const gps_map_feature_t map_features[] = {")
    lines.extend(feature_entries)
    lines.append("};")
    lines.append("")
    lines.append("static const gps_map_label_t map_labels[] = {")
    for name, lat, lon, zoom in labels:
        lines.append(f"    {{\"{name}\", {lat}, {lon}, {zoom}}},")
    lines.append("};")
    lines.append("")

    OUT_PATH.write_text("\n".join(lines) + "\n")
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    emit()
