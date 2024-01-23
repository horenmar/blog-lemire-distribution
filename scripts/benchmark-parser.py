#!/usr/bin/env python3

from collections import defaultdict, namedtuple
import csv
import os.path
import re
import sys
import xml.etree.ElementTree as ET
from typing import Optional

iters_regex = re.compile('iters=(\d+)')

def extract_iters(result_name: str) -> Optional[str]:
    match = iters_regex.search(result_name)
    if match:
        return match.group(1)
    return None


def has_benchmark(test_case):
    return test_case.find('BenchmarkResults') is not None

def make_output_name(input_name: str) -> str:
    return os.path.splitext(input_name)[0] + ".csv"

## TODO: sections in paths?
## TODO: number of elements processed?

# Assumes input is in ns
def pick_time_unit(amount: float) -> tuple[int, str]:
    NS_in_US = 1000
    NS_in_MS = 1000 * NS_in_US
    NS_in_S = 1000 * NS_in_MS
    NS_in_M = 60 * NS_in_S

    if amount < NS_in_US:
        return (1, 'ns')
    if amount < NS_in_MS:
        return (NS_in_US, 'us')
    if amount < NS_in_S:
        return (NS_in_MS, 'ms')
    if amount < NS_in_M:
        return (NS_in_S, 's')
    return (NS_in_M, 'm')

class BenchResult():
    __slots__ = ['mean', 'stddev', 'items_processed']

    def __init__(self, mean: float, stddev: float, items_processed: int = -1):
        self.mean = mean
        self.stddev = stddev
        self.items_processed = items_processed

    # Returns string representation in style "2.3 ± 0.1 ms"
    def with_unit(self) -> str:
        # the values are in nanoseconds, we want to find something human-readable
        coefficient, unit = pick_time_unit(self.mean)
        return f'{self.mean / coefficient:.2f} ± {self.mean / coefficient:.2f} {unit}'

    # Returns string representation in style "2.3 ± 0.1 ms", but per processed item
    def with_unit_per_item(self) -> str:
        adjusted_mean = self.mean
        adjusted_stddev = self.stddev
        if self.items_processed != -1:
            adjusted_mean /= self.items_processed
            adjusted_stddev /= self.items_processed

        coefficient, unit = pick_time_unit(adjusted_mean)
        adjusted_mean /= coefficient
        adjusted_stddev /= coefficient
        return f'{adjusted_mean:.2f} ± {adjusted_stddev:.2f} {unit}'

BenchInfo = namedtuple('BenchInfo', ['benchmark_name', 'bench_result'])
BenchGroup = namedtuple('BenchGroup', ['bench_type', 'bench_infos'])

def parse_to_objects(test_case) -> dict[str, list[BenchInfo]]:
    case_name = test_case.attrib['name']
    # For now we only handle benchmarks that are direct descendants, we should extend this for sections later
    bench_results = []
    for benchmark_node in test_case.findall('BenchmarkResults'):
        bench_name = benchmark_node.attrib['name']
        mean = float(benchmark_node.find('mean').attrib['value'])
        stddev = float(benchmark_node.find('standardDeviation').attrib['value'])
        items_processed = -1
        iters_in_bench = extract_iters(bench_name)
        if iters_in_bench is not None:
            items_processed = int(iters_in_bench)
        bench_results.append(BenchInfo(bench_name, BenchResult(mean, stddev, items_processed)))
    return case_name, bench_results

def group_by_test(cases_and_results: dict[str, list[BenchResult]]):
    regrouped = defaultdict(list)
    for testname, results in cases_and_results.items():
        shared_name, type_suffix = testname.split('-')
        shared_name = shared_name.strip()
        type_suffix = type_suffix.strip()
        regrouped[shared_name].append(BenchGroup(type_suffix, results))
    return regrouped

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Exactly 1 argument expected.')
        print(f'Usage: {sys.argv[0]} catch2-xml-benchmark-output')
        exit(1)
    root = ET.parse(sys.argv[1]).getroot()
    test_cases = root.findall('TestCase')
    test_cases = [x for x in test_cases if has_benchmark(x)]
#    print(len(test_cases))
    benches_by_test = dict()
    for test_case in test_cases:
        test_name, bench_results = parse_to_objects(test_case)
        benches_by_test[test_name] = bench_results
    regrouped = group_by_test(benches_by_test)

    from pprint import pprint
    with open(make_output_name(sys.argv[1]), 'w', encoding='utf-8', newline='') as f:
        csvwriter = csv.DictWriter(f, fieldnames=['bench_name', 'metadata', 'runtime', 'per_item'], delimiter=',')
        csvwriter.writeheader()
        for base_name, group in regrouped.items():
            for bench in group:
                bench_type, infos = bench
                for info in infos:
                    name, results = info
                    csvwriter.writerow({'bench_name': bench_type, 'metadata': name, 'runtime': results.with_unit(), 'per_item': results.with_unit_per_item()})
