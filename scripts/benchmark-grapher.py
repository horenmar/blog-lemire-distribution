#!/usr/bin/env python3

from collections import defaultdict
import sys
import re
import os
import plotly.graph_objects as pgo
import xml.etree.ElementTree as ET
from typing import Optional, NamedTuple, DefaultDict
from pprint import pprint

titles = {
  'emul' : 'Time to multiply together two numbers N times',
  'without-reuse': 'Time to generate a number from each range [0, 0]..[0, N)',
  'with-reuse': 'Time to generate 1M numbers from range [0, N]',
  'mod': 'Time to compute random number modulo N',
}

x_titles = {
  'emul' : 'N',
  'without-reuse': 'N',
  'with-reuse': 'N',
}

# Mapping from raw number to "semantic" string
number_renames = {
  10000                 : '10K',
  100000                : '100K',
  1000000               : '1M',
  10000000              : '10M',
  4294967294            : 'UINT32_MAX - 1',
  4294967295            : 'UINT32_MAX',
  4294967296            : 'UINT32_MAX + 1',
  68719476736           : '2**36',
  1099511627776         : '2**40',
  17592186044416        : '2**44',
  281474976710656       : '2**48',
  4503599627370496      : '2**52',
  72057594037927936     : '2**56',
  1152921504606846976   : '2**60',
  9223372036854775808   : 'UINT64_MAX / 2 + 1',
  18446744073709551614  : 'UINT64_MAX -1',
}

included_bounds = { 100, 4294967296, 9223372036854775808, 12298110947468241578, 18446744073709551614 }

INCLUDE_LESS_BOUNDS = False
USE_SHORT_PRESENTATION_NAME = False
INCLUDE_PLAIN_DISTRIBUTIONS = True
TARGET_ITERS = 1000000

def benchmark_presentation_name(bench_name: str) -> str:
    if '_' not in bench_name:
        return bench_name

    base_name, optimized_for, *rest = bench_name.split('_')
    if USE_SHORT_PRESENTATION_NAME:
        if 'lemire' in base_name:
            return f'{base_name}, {rest[1][5:-1]}'
        return f'{base_name}'
    else:
        if 'lemire' in base_name:
            return f'{base_name}, {optimized_for}, {rest[1][5:-1]}'
        return f'{base_name}, {optimized_for}'

iters_regex = re.compile('iters=(\d+)')
bounds_regex = re.compile('bounds=(\d+)')

def extract_bounds(metadata: str) -> Optional[int]:
    match = bounds_regex.search(metadata)
    if match:
        return int(match.group(1))
    return None

def has_bounds(metadata: str) -> bool:
    return extract_bounds(metadata) is not None

def extract_iters(metadata: str) -> Optional[int]:
    match = iters_regex.search(metadata)
    if match:
        return int(match.group(1))
    return None

def has_iters(metadata: str) -> bool:
    return extract_iters(metadata) is not None

def make_base_name(input_path: str) -> str:
    return os.path.splitext(os.path.basename(input_path))[0]


ResultInterval = NamedTuple('ResultInterval', value = float, low = float, high = float)
class BenchResult(NamedTuple):
    test_name: str
    bench_name: str
    mean: ResultInterval
    stddev: ResultInterval

def has_benchmark(test_case):
    return test_case.find('BenchmarkResults') is not None

def read_interval_values(node) -> ResultInterval:
    value = node.attrib['value']
    low = node.attrib['lowerBound']
    high = node.attrib['upperBound']
    return ResultInterval(float(value), float(low), float(high))

def parse_to_objects(test_case) -> list[BenchResult]:
    test_name = test_case.attrib['name']
    # For now we only handle benchmarks that are direct descendants, we should extend this for sections later
    bench_results = []
    for benchmark_node in test_case.findall('BenchmarkResults'):
        bench_name = benchmark_node.attrib['name']
        mean = read_interval_values(benchmark_node.find('mean'))
        stddev = read_interval_values(benchmark_node.find('standardDeviation'))
        bench_results.append(BenchResult(test_name, bench_name, mean, stddev))
    return bench_results

def split_result_name(name: str) -> tuple[str, str]:
    # The modulo benchmark does not have a '-' in the name, because it
    # is not parametrized by anything.
    if '-' not in name:
        return name.strip(), name.strip()
    base, actual = name.split('-')
    return (base.strip(), actual.strip())

# TODO: returns BenchResult grouped by the test name, with later processing filtering further
def group_results_by_type(results: list[BenchResult]) -> DefaultDict[str, list[BenchResult]]:
    # The test names use simple pattern '{the basic name} - {benchmarked type}'
    # We check that all results are from the same basic test and then group
    # them by the type part
    expected_base_name = split_result_name(results[0].test_name)[0]
    assert all(r.test_name.startswith(expected_base_name) for r in results)
    grouped = defaultdict(list)
    for result in results:
        grouped[split_result_name(result.test_name)[1]].append(result)
    return grouped

PlotData = NamedTuple('PlotData', xs = list[int], ys = list[float], xs_low = float, xs_high = float)

def map_results_to_data(results: list[BenchResult]) -> PlotData:
    if has_bounds(results[0].bench_name):
        results = [r for r in results if extract_iters(r.bench_name) == TARGET_ITERS]
    assert len(results) > 0
    data = []
    for result in results:
        bounds = extract_bounds(result.bench_name)
        iters = extract_iters(result.bench_name)
        if bounds is not None:
            # The full graph is massive, so we also want a secondary one
            # with only few bounds included
            if INCLUDE_LESS_BOUNDS and bounds not in included_bounds:
                continue
            x = bounds
        else:
            x = iters
        values = result.mean
        normalized_values = ResultInterval(values.value / iters, values.low / iters, values.high / iters)
        data.append((x, normalized_values))
    data.sort()
    return PlotData([x[0] for x in data],
                    [x[1].value for x in data],
                    [x[1].low for x in data],
                    [x[1].high for x in data])

def benchmark_order(bench: str) -> int:
    # This is a terrible hack for imposing order on the benchmarks in graph
    # Thankfully we don't have many of them
    offset = 0
    # we want 'reuse' benchmarks after no-reuse
    if 'reuse' in bench:
        offset = 5
    if 'BSD' in bench:
        return offset + 0
    if 'java' in bench:
        return offset + 1
    # These capture both emul benchmarks and lemire distribution benchmark
    if 'NaiveMult' in bench:
        return offset + 2
    if 'OptimizedMult' in bench:
        return offset + 3
    if 'IntrinsicMult' in bench:
        return offset + 4
    # unknown, let it fall where it may
    print(f"Unknown bench '{bench}'")
    return offset

def order_benchmarks(benchmarks: list[str]) -> list[str]:
    correct_order = sorted(benchmarks, key=benchmark_order)
    return correct_order


def make_bar_chart(benchmark: str, data: PlotData):
    interval_below = [value - value_low for value, value_low in zip(data.ys, data.xs_low)]
    return pgo.Bar(
        name = f'<b>{benchmark_presentation_name(benchmark)}</b>',
        # We prefer the bold text for names, but then they are sorted
        # implicitly by insertion order.
#        x = data.xs,
        x = [f'<b>{number_renames.get(x, x)}</b>' for x in data.xs],
        text = [f'<b>{y:.1f}</b>' for y in data.ys],
        textposition = 'outside',
        y = data.ys,
        # Turns out that the confidence interval is so tiny as to be unreadable.
#        error_y = dict(
#          type = 'data',
#          arrayminus = [value - value_low for value, value_low in zip(data.ys, data.xs_low)],
#          array = [value_high - value for value, value_high in zip(data.ys, data.xs_high)],
#        )
    )

def make_figure(bars, title_text, x_title, save_suffix):
    fig = pgo.Figure(data=bars)
    fig.update_layout(
      title = {
          'text': f'<b>{title_text}</b>',
          'x': 0.5,
          'xanchor': 'center'
      },
      yaxis_title = 'ns per number',
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


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Exactly 1 argument expected.')
        print(f'Usage: {sys.argv[0]} catch2-xml-benchmark-output')
        exit(1)
    base_name = make_base_name(sys.argv[1])
    root = ET.parse(sys.argv[1]).getroot()
    test_cases = [x for x in root.findall('TestCase') if has_benchmark(x)]
    results = []
    for test_case in test_cases:
        bench_results = parse_to_objects(test_case)
        results.extend(bench_results)

    grouped = group_results_by_type(results)
    for_plotting = { benchmark : map_results_to_data(results) for benchmark, results in grouped.items() }
    ordered = order_benchmarks(list(for_plotting.keys()))
    if not INCLUDE_PLAIN_DISTRIBUTIONS:
        ordered = [bench for bench in ordered if not 'plain' in bench]

    bars = [make_bar_chart(bench, for_plotting[bench]) for bench in ordered]
    make_figure(bars, titles.get(base_name, base_name), x_titles.get(base_name, 'N'), base_name)
