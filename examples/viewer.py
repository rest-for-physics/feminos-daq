#!/usr/bin/env python3

import tkinter as tk
from tkinter import filedialog, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import uproot
import awkward as ak


def get_event(tree: uproot.TTree, entry: int):
    if entry >= tree.num_entries:
        raise ValueError(f"Entry {entry} is out of bounds. Tree has {tree.num_entries} entries.")

    events = tree.arrays(entry_start=entry, entry_stop=entry + 1)
    events["signal_data"] = ak.unflatten(events["signal_data"], 512, axis=1)

    signals = ak.Array({"id": events["signal_ids"], "data": events["signal_data"]}, with_name="Signals")
    events["signals"] = signals

    events = ak.without_field(events, "signal_ids")
    events = ak.without_field(events, "signal_data")

    return events[0]


class GraphApp:
    def __init__(self, _root):
        self.file = None
        self.event_tree = None
        self.run_tree = None

        self.root = _root
        self.root.title("Graph Plotter")

        self.label = tk.Label(self.root, text="Select a CSV file to plot graphs:")
        self.label.pack(pady=10)

        self.open_button = tk.Button(self.root, text="Open File", command=self.open_file)
        self.open_button.pack(pady=10)

        self.plot_button = tk.Button(self.root, text="Plot Graph", command=self.plot_graph)
        self.plot_button.pack(pady=10)

        self.filepath = None

    def open_file(self):
        self.filepath = filedialog.askopenfilename(
            filetypes=[("ROOT files", "*.root")]
        )
        if not self.filepath:
            return

        self.file = uproot.open(self.filepath)

        try:
            self.event_tree = self.file["events"]
        except KeyError:
            messagebox.showerror("Error",
                                 f"The file does not contain the 'events' tree. file keys are {self.file.keys()}")
            return

        try:
            self.run_tree = self.file["run"]
        except KeyError:
            messagebox.showerror("Error", f"The file does not contain the 'run' tree. file keys are {self.file.keys()}")
            return

    def plot_graph(self):
        if self.filepath is None or self.event_tree is None or self.run_tree is None:
            messagebox.showwarning("No File", "Please select a file first!")
            return

        try:
            plt.figure(figsize=(8, 6))

            event = get_event(self.event_tree, 0)

            for id, data in zip(event.signals.id, event.signals.data):
                plt.plot(data, label=f"Signal {id}", alpha=0.5, linewidth=0.5)
                break

            plt.xlabel("Time bins")
            plt.ylabel("ADC")

            self.show_plot()

        except Exception as e:
            messagebox.showerror("Error", f"An error occurred while plotting: {str(e)}")

    def show_plot(self):
        figure = plt.gcf()
        figure_canvas = FigureCanvasTkAgg(figure, self.root)
        figure_canvas.get_tk_widget().pack()
        figure_canvas.draw()


if __name__ == "__main__":
    root = tk.Tk()
    app = GraphApp(root)
    root.mainloop()
