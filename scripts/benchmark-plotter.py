#!/usr/bin/env python3

from collections import defaultdict
import csv
import plotly.graph_objects as go
import plotly.io as pio
from pprint import pprint
import re
import sys
import os

from typing import Optional

iters_regex = re.compile('iters=(\d+)')
bounds_regex = re.compile('bounds=(\d+)')

def extract_bounds(metadata: str) -> Optional[str]:
    match = bounds_regex.search(metadata)
    if match:
        return match.group(1)
    return None

def extract_iters(metadata: str) -> Optional[str]:
    match = iters_regex.search(metadata)
    if match:
        return match.group(1)
    return None
    
def make_output_name(input_path: str) -> str:
    return os.path.splitext(os.path.basename(input_path))[0]


# Mapping from raw number to "semantic" string
number_renames = {
  '100000' : '100K',
  '1000000' : '1M',
  '10000000': '10M',
  '4294967294'           : 'UINT32_MAX - 1',
  '4294967295'           : 'UINT32_MAX',
  '4294967296'           : 'UINT32_MAX + 1',
  '68719476736'          : '2**36',
  '1099511627776'        : '2**40',
  '17592186044416'       : '2**44',
  '281474976710656'      : '2**48',
  '4503599627370496'     : '2**52',
  '72057594037927936'    : '2**56',
  '1152921504606846976'  : '2**60',
  '9223372036854775808'  : 'UINT64_MAX / 2 + 1',
  '18446744073709551614' : 'UINT64_MAX -1',
}

def number_presentation_string(number: int) -> str:
    return number_renames.get(str(number), str(number))

def distribution_order(distribution: str) -> int:
    # This is a terrible hack for imposing order on the distributions in graph
    # Thankfully we don't have many of them
    offset = 0
    if 'reuse' in distribution:
        offset = 5
    if 'BSD' in distribution:
        return offset + 0
    if 'java' in distribution:
        return offset + 1
    if 'lemire' and 'NaiveMult' in distribution:
        return offset + 2
    if 'lemire' and 'OptimizedMult' in distribution:
        return offset + 3
    if 'lemire' and 'IntrinsicMult' in distribution:
        return offset + 4
    # unknown, let it fall where it may
    print(f"Unknown distribution '{distribution}'")
    return offset

def distribution_presentation_name(distribution: str) -> str:
    base_name, optimized_for, *rest = distribution.split('_')
    if 'lemire' in base_name:
        return f'{base_name}, {optimized_for}, {rest[1][5:-1]}'
    return f'{base_name}, {optimized_for}'
#    if 'lemire' in base_name:
#        return f'{base_name}, {rest[1][5:-1]}'
#    return f'{base_name}'

def reorder_distributions(distributions):
    # Assume the list of (distribution-name, [data]) shape of inputs
    correct_order = sorted(
        (distribution_order(dist_data[0]), dist_data) for dist_data in distributions
    )
    return [x[1] for x in correct_order]

def make_bar_chart(distribution, data):
    data = sorted(data)
    return go.Bar(
        name = f'<b>{distribution_presentation_name(distribution)}</b>',
        x = [f'<b>{number_presentation_string(d[0])}</b>' for d in data],
        y = [d[1] for d in data],
        text = [f'<b>{d[1]:.1f}</b>' for d in data],
        textposition = 'outside'
    )

def make_figure(bars, title_text, x_title, save_suffix):
    fig = go.Figure(data=bars)
    fig.update_layout(
      title = {
          'text': f'<b>{title_text} (ns)</b>',
          'x': 0.5,
          'xanchor': 'center'
      },
      yaxis_title = 'time per number',
      xaxis_title = f'<b>{x_title}</b>',
      font = {
        'family': 'Courier New, monospace',
        'size': 18,
      },
      barmode = 'group',
      legend_title_text = '<b>Distribution</b>',
    )
    fig.update_xaxes(type='category')
    config = {
      'toImageButtonOptions': {
        'format': 'png', # We don't want svg/webp due to lesser browser support
        'filename': f'{save_suffix}',
        'height': 1080,
        'width': 2560,
      }
    }

    fig.write_html(f'graph-{save_suffix}.html', include_plotlyjs='cdn', include_mathjax=False, full_html=True, validate=True, config=config)
    # Insert graph doesn't work well with the blog theme
#    fig.write_html(f'insert-graph-{save_suffix}.html', include_plotlyjs='cdn', include_mathjax=False, full_html=False, validate=True)

def parse_noreuse(reader):
    results = defaultdict(list)
    for row in reader:
        distribution = row['bench_name']
        iters = extract_iters(row['metadata'])
        if iters is None:
            print(f"Could not extract iters from noreuse csv! '{row}'")
        assert row['per_item'].endswith('ns'), f"We assume nanoseconds, but got different per_item runtime: '{row}'"
        per_item = row['per_item'].split(' ± ')[0]
        results[distribution].append((int(iters), float(per_item)))
    return list(results.items())

def parse_reuse(reader):
    results_by_iteration = defaultdict(lambda: defaultdict(list))
    for row in reader:
        distribution = row['bench_name']
        bounds = extract_bounds(row['metadata'])
        iters = extract_iters(row['metadata'])
        if iters is None:
            print(f"Could not extract iters from reuse csv! '{row}'")
        if bounds is None:
            print(f"Could not extract bounds from reuse csv! '{row}'")
        assert row['per_item'].endswith('ns'), f"We assume nanoseconds, but got different per_item runtime: '{row}'"
        per_item = row['per_item'].split(' ± ')[0]
        results_by_iteration[int(iters)][distribution].append((int(bounds), float(per_item)))

    target_iters = 1000000
#    return list(x for x in results_by_iteration[target_iters].items() if 'reuse' in x[0])
    return list(results_by_iteration[target_iters].items())

handle_reuse = True

if len(sys.argv) != 2:
    print('Exactly 1 argument expected.')
    print(f'Usage: {sys.argv[0]} csv-output-from-the-parser')
    exit(1)

with open(sys.argv[1], 'r', encoding='utf-8') as f:
    reader = csv.DictReader(f, delimiter=',')
    if handle_reuse:
        parsed = parse_reuse(reader)
    else:
        parsed = parse_noreuse(reader)

bars = []
for distribution, data in reorder_distributions(parsed):
    bars.append(make_bar_chart(distribution, data))

if handle_reuse:
    make_figure(bars, 'Time to generate 1M numbers from range [0, N]', 'N', make_output_name(sys.argv[1]))
else:
    make_figure(bars, 'Time to generate a number from each range [0, 0]..[0, N)', 'N', make_output_name(sys.argv[1]))
