import sys
import os
sys.path.insert(1, '..')
import protos.experiment_pb2 as protos
import configparser
import google.protobuf.text_format as text_format
import matplotlib.pyplot as plt
from typing import List

# Read the configuration
config = configparser.ConfigParser()
config.read('plot.ini')
prefix = eval(config.get('dirs', 'prefix'))
suffix_s = eval(config.get('dirs', 'paths'))
output = eval(config.get('dirs', 'output_file'))

nodes = eval(config.get('desc', 'node_count'))
threads = eval(config.get('desc', 'thread_count'))

title = eval(config.get('desc', 'title'))
subtitle = eval(config.get('desc', 'subtitle'))
x_axis = eval(config.get('desc', 'x_axis'))
x_label = eval(config.get('desc', 'x_label'))

# Get the protos into a list
proto = {}
for suffix in suffix_s:
    for file in os.listdir(os.path.join(prefix, suffix)):
        if not file.__contains__("pbtxt"):
            continue
        f = open(os.path.join(prefix, suffix, file), "rb")
        p = text_format.Parse(f.read(), protos.ResultProto())
        node_c =  p.params.node_count
        thread_c =  p.params.thread_count
        if node_c not in proto:
            proto[node_c] = {}
        if thread_c not in proto[node_c]:
            proto[node_c][thread_c] = {}
        proto[node_c][thread_c][file] = p
        f.close()

# Graph the nodes
x, y = [], []
for n in nodes:
    for t in threads:
        one_tick = x_axis
        one_tick = one_tick.replace("$node$", str(n))
        one_tick = one_tick.replace("$thread$", str(t))
        x.append(one_tick)
        mean_total = 0
        for node_file, node_data in proto[n][t].items():
            count = 0
            for it in node_data.driver:
                count += 1
                mean_total += it.qps.summary.mean
            assert(count == t)
        assert(len(proto[n][t]) == n)
        y.append(mean_total)

fig = plt.figure(figsize=(8, 8))
plt.suptitle(title)
plt.title(subtitle)
locs, labels = plt.xticks()
plt.setp(labels, rotation=45)
plt.xlabel(x_label)
plt.ylabel("Total Throughput (ops/second)")
plt.scatter(x, y, marker='x', color='r', s=60)
plt.savefig(output)