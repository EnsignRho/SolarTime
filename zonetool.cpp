//////////
//
// zonetool.cpp -- publish the zone registry (spec §8):  read data/zones.tsv, build the registry
// once (merge overlapping discs, then every merged zone's jump and ramp), and write the file back
// with ramp_km and jump_s filled in and each row's merged zone given (zone:  the row of its leader,
// 0 = the first city;  zone_name:  the leader by name) -- so devices load it at once instead of
// computing it.  Prints a report of the merged zones.  Run it again after any edit to the rows.
//
// Usage:  soltime_zonetool <zones.tsv in> <zones.tsv out>
//
// UNITY BUILD over soltime.cpp (the registry's internals give the report its numbers).
//
//////
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <time.h>
	#include <errno.h>
	#include <ctype.h>
	#include <unistd.h>
	#include <sys/stat.h>
	#include <fstream>
	#include <string>
	#include "soltime.cpp"


//////////
// Rows
//////
// BUG_FIX_PLAN_2026-10-05.md FIX-04 — fixed 2026-10-05 16:46:30 EDT (UTC-04:00).
// Owned strings let every validation failure exit without leaking partially parsed rows.
	struct SRow
	{
		std::string line, name, country, admin1;
		SSolZone z;
	};

	static bool iNumber(const std::string& field, double* value)
	{
		if (field.empty() || isspace((unsigned char)field.front()) || isspace((unsigned char)field.back())) return false;
		char* end;
		errno = 0;
		*value = strtod(field.c_str(), &end);
		return end != field.c_str() && *end == 0 && errno != ERANGE && isfinite(*value);
	}

	int main(int argc, char* argv[])
	{
		setvbuf(stdout, NULL, _IOLBF, 0);
		if (argc < 3)
		{
			fprintf(stderr, "usage:  soltime_zonetool <zones.tsv in> <zones.tsv out>\n");
			return 2;
		}

		std::ifstream input(argv[1]);
		if (!input) { fprintf(stderr, "soltime_zonetool: cannot read %s\n", argv[1]); return 1; }
		std::vector<SRow> rows;
		std::vector<std::string> com;
		std::string line;
		size_t lineNumber = 0;
		while (std::getline(input, line))
		{
			++lineNumber;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			auto invalid = [&](const char* reason) {
				fprintf(stderr, "soltime_zonetool: %s:%zu: %s\n", argv[1], lineNumber, reason);
				return 1;
			};
			if (line.find('\0') != std::string::npos) return invalid("embedded NUL byte");
			if (line.empty()) continue;
			if (line[0] == '#') {
				if (line.compare(0, 7, "# zone:") != 0) com.push_back(line);
				continue;
			}
			std::vector<std::string> fields;
			size_t start = 0, tab;
			do {
				tab = line.find('\t', start);
				fields.push_back(line.substr(start, tab == std::string::npos ? tab : tab - start));
				start = tab == std::string::npos ? line.size() : tab + 1;
			} while (tab != std::string::npos);
			if (fields.size() != 11 && fields.size() != 13) return invalid("expected 11 raw or 13 published columns");
			if (fields[0].empty() || fields[1].size() != 2 || fields[7].empty())
				return invalid("name, two-letter country and radius_source are required");
			double values[12] = {};
			for (int column : {3, 4, 5, 6, 8, 9, 10, 11}) {
				if (column == 11 && fields.size() == 11) continue;
				if ((column == 9 || column == 10) && fields[column] == "-") continue;
				if (!iNumber(fields[column], &values[column])) {
					char reason[96]; snprintf(reason, sizeof(reason), "column %d must contain a finite number", column + 1);
					return invalid(reason);
				}
			}
			if (fabs(values[3]) > 90 || fabs(values[4]) > 180) return invalid("latitude/longitude out of range (columns 4/5)");
			if (values[5] < 0 || floor(values[5]) != values[5]) return invalid("population must be a nonnegative integer (column 6)");
			if (values[6] <= 0 || values[8] <= 0) return invalid("city and zone radii must be positive (columns 7/9)");
			if (values[9] < 0 || values[10] < 0) return invalid("derived ramp/jump must be nonnegative or '-' (columns 10/11)");
			if (fields.size() == 13 && (values[11] < 0 || floor(values[11]) != values[11] || fields[12].empty()))
				return invalid("published leader index/name invalid (columns 12/13)");
			SRow row;
			row.name = fields[0]; row.country = fields[1]; row.admin1 = fields[2];
			for (int i = 0; i < 9; ++i) { if (i) row.line += '\t'; row.line += fields[i]; }
			// Derived columns are validated but always recalculated after edits.
			row.z = {values[3], values[4], 0.0, values[8], -1.0, values[5], 0.0};
			rows.push_back(row);
		}
		if (input.bad()) { fprintf(stderr, "soltime_zonetool: read failed for %s\n", argv[1]); return 1; }
		input.close();
		if (rows.empty()) { fprintf(stderr, "soltime_zonetool: %s: registry contains no cities\n", argv[1]); return 1; }
		s32 nRows = (s32)rows.size(), nCom = (s32)com.size();

		// The registry
		SSolZone*	zones = new SSolZone[nRows > 0 ? nRows : 1];
		for (s32 i = 0; i < nRows; ++i)		zones[i] = rows[i].z;
		clock_t			c0	= clock();
		SSolRegistry*	reg	= soltime_registryCreate(NULL, zones, nRows);
		f64				secs = (f64)(clock() - c0) / CLOCKS_PER_SEC;
		if (!reg) { delete[] zones; fprintf(stderr, "soltime_zonetool: the registry was refused\n"); return 1; }

		// BUG_FIX_PLAN_2026-10-05.md FIX-04 — fixed 2026-10-05 16:46:30 EDT (UTC-04:00).
		// Validate/build first, then publish atomically in the destination directory.
		// Failed writes and in-place invalid rebuilds must leave the old registry intact.
		std::string pattern = std::string(argv[2]) + ".tmp.XXXXXX";
		std::vector<char> temp(pattern.begin(), pattern.end()); temp.push_back(0);
		int fd = mkstemp(temp.data());
		FILE* o = fd < 0 ? NULL : fdopen(fd, "w");
		if (!o) {
			if (fd >= 0) { close(fd); unlink(temp.data()); }
			soltime_registryDelete(reg); delete[] zones;
			fprintf(stderr, "soltime_zonetool: cannot create output beside %s\n", argv[2]); return 1;
		}
		struct stat existing;
		mode_t mode;
		if (stat(argv[2], &existing) == 0) mode = existing.st_mode & 0777;
		else { mode_t mask = umask(0); umask(mask); mode = 0666 & ~mask; }
		bool writeOk = fchmod(fd, mode) == 0;
		for (s32 i = 0; i < nCom; ++i)
		{
			if (strncmp(com[i].c_str(), "# name\t", 7) == 0)
			{
				fprintf(o, "# zone:  the row (0 = the first city) whose clock this city keeps -- its merged zone's most populous city\n");
				fprintf(o, "# name\tcountry\tadmin1\tlat\tlon\tpopulation\tcity_radius_km\tradius_source\tzone_radius_km\tramp_km\tjump_s\tzone\tzone_name\n");
			} else {
				fprintf(o, "%s\n", com[i].c_str());
			}
		}
		for (s32 i = 0; i < nRows; ++i)
		{
			s32	L = soltime_zoneLeader(reg, i);
			if (!rows[L].admin1.empty())	fprintf(o, "%s\t%.6f\t%.6f\t%d\t%s, %s, %s\n", rows[i].line.c_str(), soltime_zoneRampKm(reg, i), soltime_zoneJumpSecs(reg, i), L, rows[L].name.c_str(), rows[L].admin1.c_str(), rows[L].country.c_str());
			else					fprintf(o, "%s\t%.6f\t%.6f\t%d\t%s, %s\n",     rows[i].line.c_str(), soltime_zoneRampKm(reg, i), soltime_zoneJumpSecs(reg, i), L, rows[L].name.c_str(), rows[L].country.c_str());
		}
		writeOk = !ferror(o) && writeOk;
		if (fflush(o) != 0) writeOk = false;
		if (writeOk && fsync(fd) != 0) writeOk = false;
		if (fclose(o) != 0) writeOk = false;
		if (!writeOk || rename(temp.data(), argv[2]) != 0) {
			unlink(temp.data()); soltime_registryDelete(reg); delete[] zones;
			fprintf(stderr, "soltime_zonetool: cannot publish %s; existing output preserved\n", argv[2]); return 1;
		}

		// The report
		s32	singles = 0;
		for (s32 pos = 0; pos < reg->nGroups; ++pos)
			if (reg->memberStart[pos + 1] - reg->memberStart[pos] == 1)		++singles;
		printf("zones %d -> merged zones %d (%d single cities);  built in %.1f s\n", nRows, reg->nGroups, singles, secs);

		// Widest merged zones:  the farthest disc edge from the leader's center
		f64*	reach	= new f64[reg->nGroups];
		s32*	byReach	= new s32[reg->nGroups];
		for (s32 pos = 0; pos < reg->nGroups; ++pos)
		{
			s32	L	= reg->order[pos];
			f64	far	= 0.0;
			for (s32 j = reg->memberStart[pos]; j < reg->memberStart[pos + 1]; ++j)
			{
				s32	m = reg->members[j];
				far = fmax(far, iSolTime_distanceKm(zones[L].latDeg, zones[L].lonDegE, zones[m].latDeg, zones[m].lonDegE) + reg->lockKm[m]);
			}
			reach[pos]		= far;
			byReach[pos]	= pos;
		}
		std::sort(byReach, byReach + reg->nGroups, [reach](s32 a, s32 b) { return reach[a] > reach[b]; });
		printf("widest merged zones (reach:  the farthest disc edge from the leader's center;  accuracy:  J, the most anyone in it is off their own SolTime):\n");
		for (s32 j = 0; j < reg->nGroups && j < 15; ++j)
		{
			s32	pos = byReach[j], L = reg->order[pos];
			printf("  %-24s %s  %3d cities  reach %5.0f km (%4.0f mi)  accuracy %5.1f min  ramp %5.1f km\n", rows[L].name.c_str(), rows[L].country.c_str(),
				   reg->memberStart[pos + 1] - reg->memberStart[pos], reach[pos], reach[pos] / 1.609344, reg->jump[L] / 60.0, reg->rampKm[L]);
		}

		// Accuracy spread
		f64*	J = new f64[reg->nGroups];
		for (s32 pos = 0; pos < reg->nGroups; ++pos)	J[pos] = reg->jump[reg->order[pos]];
		std::sort(J, J + reg->nGroups);
		printf("accuracy (J) over the merged zones:  median %.1f min, 90%% %.1f min, worst %.1f min\n",
			   J[reg->nGroups / 2] / 60.0, J[(s32)(reg->nGroups * 0.9)] / 60.0, J[reg->nGroups - 1] / 60.0);

		// The U.S.
		s32	us = 0, usGroups = 0;
		for (s32 i = 0; i < nRows; ++i)
		{
			if (strcmp(rows[i].country.c_str(), "US") != 0)		continue;
			++us;
			if (soltime_zoneLeader(reg, i) == i)		++usGroups;
		}
		printf("U.S.:  %d cities -> %d merged zones\n", us, usGroups);

		delete[] J;
		delete[] reach;
		delete[] byReach;
		soltime_registryDelete(reg);
		delete[] zones;
		return 0;
	}
