#!/usr/bin/env python3
"""Produce a compact NOAA tide station catalog for the TideLantern phone UI.

Runs as part of GitHub Actions (with network access). A checked-in small seed
catalog is the offline fallback. This is separate from the firmware binary.
"""
import json
from pathlib import Path
from datetime import datetime, timezone
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parent.parent
SEED = ROOT / 'data/seed_stations.json'
OUTPUT = ROOT / 'web-installer/tide-stations.json'
URL = 'https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=tidepredictions'


def catalog():
    req = Request(URL, headers={'User-Agent': 'TideLantern/0.1 (+https://github.com/Tikishark777/TideLantern)'})
    with urlopen(req, timeout=30) as response:
        data = json.load(response)
    result = []
    for station in data['stations']:
        try:
            station_id = str(station['id']).strip()
            if not station_id.isalnum() or len(station_id) > 10:
                continue
            result.append({'id': station_id, 'name': str(station['name']).strip()[:90],
                           'lat': round(float(station['lat']), 5),
                           'lon': round(float(station['lng']), 5),
                           'type': str(station.get('type', ''))[:1]})
        except (ValueError, TypeError, KeyError):
            continue
    if len(result) < 1000:
        raise ValueError(f'Only {len(result)} NOAA stations found; using fallback instead')
    return result, 'NOAA CO-OPS Metadata API'


def main():
    try:
        stations, source = catalog()
    except Exception as exc:
        print('WARNING: Could not fetch full NOAA station catalog:', exc)
        stations, source = json.loads(SEED.read_text()), 'Bundled limited fallback list'
    payload = {'generated_utc': datetime.now(timezone.utc).isoformat(), 'source': source, 'stations': stations}
    OUTPUT.write_text(json.dumps(payload, separators=(',', ':'), ensure_ascii=False) + '\n', encoding='utf-8')
    print(f'Generated {len(stations)} NOAA stations ({source}) at {OUTPUT}')


if __name__ == '__main__':
    main()
