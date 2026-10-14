# Data attribution

`places.tsv` and `countries.tsv` are derived from the **GeoNames** geographical database,
<https://www.geonames.org/>, licensed under the
[Creative Commons Attribution 4.0 License](https://creativecommons.org/licenses/by/4.0/).

- Source files:  `cities15000.txt` (every place with a population of 15,000 or more),
  `admin1CodesASCII.txt`, `countryInfo.txt`, from <https://download.geonames.org/export/dump/>,
  downloaded Oct.03.2026.
- Changes:  columns reduced to name, ASCII name, country, state/province, latitude, longitude,
  population and feature code; sorted by country and population (`make_places.pl` rebuilds them).

`zones.tsv` (every city of 100,000+ people and its zone) is built from `places.tsv` and two
more sources:

- **GHSL Urban Centre Database R2024A**, general characteristics theme (V1-2), from the
  European Commission's Joint Research Centre, <https://human-settlement.emergency.copernicus.eu/>.
  (c) European Union, 2024, licensed under the
  [Creative Commons Attribution 4.0 License](https://creativecommons.org/licenses/by/4.0/).
  Downloaded Oct.03.2026.  Changes:  only each urban centre's names, country, area and
  centroid are used (the GeoPackage, `GHS_UCDB_THEME_GENERAL_CHARACTERISTICS_GLOBE_R2024A.gpkg`);
  the area becomes a city radius (sqrt(area / pi)) on the GeoNames place it matches by name, near
  the centroid.
- **U.S. Census Bureau, 2023 Gazetteer Files** (national places), public domain,
  <https://www.census.gov/geographies/reference-files/time-series/geo/gazetteer-files.html>.
  Downloaded Oct.03.2026.  Changes:  only each place's name, state, land area and location are
  used; the land area becomes a city radius on the matching GeoNames place.
- The ramp, jump and zone columns are SolTime's own (`soltime_zonetool`).  `make_zones.pl`
  rebuilds the file (its header lists the steps).

Apps that ship this data show the credit lines:
**Place data:  GeoNames (geonames.org), CC BY 4.0.**
**City areas:  GHSL Urban Centre Database R2024A, (c) European Union, CC BY 4.0;  U.S. Census Bureau Gazetteer.**

Where one line is all there is room for (the desktop display):
**Data:  GeoNames (CC BY 4.0);  GHSL (c) European Union (CC BY 4.0);  U.S. Census Bureau**
