package com.lebeli.soltime;

// Location quality policy, kept independent of Android so it can be regression-tested.
final class LocationFixes {
    static final double MAX_AGE_SECS = 60;
    static final double PREDICT_SECS = 5;
    static final class Fix {
        final double lat, lon, accuracy, elapsed, speed, heading;
        Fix(double lat, double lon, double accuracy, double elapsed, double speed, double heading) {
            this.lat = lat; this.lon = lon; this.accuracy = accuracy; this.elapsed = elapsed;
            this.speed = speed; this.heading = heading;
        }
    }
    Fix latest;
    boolean accept(Fix fix, double now) {
        double age = now - fix.elapsed;
        if (!Double.isFinite(fix.lat) || !Double.isFinite(fix.lon) || Math.abs(fix.lat) > 90
                || Math.abs(fix.lon) > 180 || !Double.isFinite(fix.accuracy) || fix.accuracy <= 0
                || !Double.isFinite(age) || age < 0 || age > MAX_AGE_SECS) return false;
        if (latest != null) {
            if (fix.elapsed <= latest.elapsed) return false;
            // A fresh precise fix should not be displaced by a coarse network result.
            if (now - latest.elapsed < PREDICT_SECS && fix.accuracy > latest.accuracy * 2) return false;
        }
        latest = fix;
        return true;
    }
    double age(double now) { return latest == null ? Double.POSITIVE_INFINITY : Math.max(0, now - latest.elapsed); }
    double speed(double now) {
        return latest != null && age(now) < PREDICT_SECS && Double.isFinite(latest.speed)
                && latest.speed >= 0 && Double.isFinite(latest.heading) ? latest.speed : 0;
    }
}
