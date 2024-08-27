#!/usr/bin/env python3

from __future__ import annotations

import tkinter as tk
from tkinter import filedialog, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import uproot
import awkward as ak
import requests
import re
import numpy as np
import matplotlib.colors as mcolors

plt.rcParams['axes.prop_cycle'] = plt.cycler(color=plt.cm.Set1.colors)

signal_id_readout_mapping = {
    4323: ("X", 30),
    4324: ("X", 29.5),
    4325: ("X", 29),
    4326: ("X", 28.5),
    4327: ("X", 28),
    4328: ("X", 27.5),
    4329: ("X", 27),
    4330: ("X", 26.5),
    4331: ("X", 26),
    4332: ("X", 25.5),
    4334: ("X", 25),
    4335: ("X", 24.5),
    4336: ("X", 24),
    4337: ("X", 23.5),
    4339: ("X", 23),
    4340: ("X", 22.5),
    4341: ("X", 22),
    4342: ("X", 21.5),
    4343: ("X", 21),
    4345: ("X", 20.5),
    4346: ("X", 20),
    4347: ("X", 19.5),
    4348: ("X", 19),
    4349: ("X", 18.5),
    4350: ("X", 18),
    4351: ("X", 17.5),
    4352: ("X", 17),
    4353: ("X", 16.5),
    4354: ("X", 16),
    4355: ("X", 15.5),
    4356: ("X", 15),
    4357: ("X", 14.5),
    4358: ("X", 14),
    4359: ("X", 13.5),
    4360: ("X", 13),
    4361: ("X", 12.5),
    4362: ("X", 12),
    4363: ("X", 11.5),
    4364: ("X", 11),
    4365: ("X", 10.5),
    4366: ("X", 10),
    4368: ("X", 9.5),
    4369: ("X", 9),
    4370: ("X", 8.5),
    4371: ("X", 8),
    4372: ("X", 7.5),
    4374: ("X", 7),
    4375: ("X", 6.5),
    4376: ("X", 6),
    4377: ("X", 5.5),
    4379: ("X", 5),
    4380: ("X", 4.5),
    4381: ("X", 4),
    4382: ("X", 3.5),
    4383: ("X", 3),
    4384: ("X", 2.5),
    4385: ("X", 2),
    4386: ("X", 1.5),
    4387: ("X", 1),
    4388: ("X", 0.5),
    4395: ("X", 0),
    4396: ("X", -0.5),
    4397: ("X", -1),
    4398: ("X", -1.5),
    4399: ("X", -2),
    4400: ("X", -2.5),
    4401: ("X", -3),
    4402: ("X", -3.5),
    4403: ("X", -4),
    4404: ("X", -4.5),
    4406: ("X", -5),
    4407: ("X", -5.5),
    4408: ("X", -6),
    4409: ("X", -6.5),
    4411: ("X", -7),
    4412: ("X", -7.5),
    4413: ("X", -8),
    4414: ("X", -8.5),
    4415: ("X", -9),
    4417: ("X", -9.5),
    4418: ("X", -10),
    4419: ("X", -10.5),
    4420: ("X", -11),
    4421: ("X", -11.5),
    4422: ("X", -12),
    4423: ("X", -12.5),
    4424: ("X", -13),
    4425: ("X", -13.5),
    4426: ("X", -14),
    4427: ("X", -14.5),
    4428: ("X", -15),
    4429: ("X", -15.5),
    4430: ("X", -16),
    4431: ("X", -16.5),
    4432: ("X", -17),
    4433: ("X", -17.5),
    4434: ("X", -18),
    4435: ("X", -18.5),
    4436: ("X", -19),
    4437: ("X", -19.5),
    4438: ("X", -20),
    4440: ("X", -20.5),
    4441: ("X", -21),
    4442: ("X", -21.5),
    4443: ("X", -22),
    4444: ("X", -22.5),
    4446: ("X", -23),
    4447: ("X", -23.5),
    4448: ("X", -24),
    4449: ("X", -24.5),
    4451: ("X", -25),
    4452: ("X", -25.5),
    4453: ("X", -26),
    4454: ("X", -26.5),
    4455: ("X", -27),
    4456: ("X", -27.5),
    4457: ("X", -28),
    4458: ("X", -28.5),
    4459: ("X", -29),
    4460: ("X", -29.5),
    4467: ("Y", 29.125),
    4468: ("Y", 29.9375),
    4469: ("Y", 28.125),
    4470: ("Y", 28.625),
    4471: ("Y", 27.125),
    4472: ("Y", 27.625),
    4473: ("Y", 26.125),
    4474: ("Y", 26.625),
    4475: ("Y", 25.125),
    4476: ("Y", 25.625),
    4478: ("Y", 24.125),
    4479: ("Y", 24.625),
    4480: ("Y", 23.125),
    4481: ("Y", 23.625),
    4483: ("Y", 22.125),
    4484: ("Y", 22.625),
    4485: ("Y", 21.125),
    4486: ("Y", 21.625),
    4487: ("Y", 20.125),
    4489: ("Y", 20.625),
    4490: ("Y", 19.125),
    4491: ("Y", 19.625),
    4492: ("Y", 18.125),
    4493: ("Y", 18.625),
    4494: ("Y", 17.125),
    4495: ("Y", 17.625),
    4496: ("Y", 16.125),
    4497: ("Y", 16.625),
    4498: ("Y", 15.125),
    4499: ("Y", 15.625),
    4500: ("Y", 14.125),
    4501: ("Y", 14.625),
    4502: ("Y", 13.125),
    4503: ("Y", 13.625),
    4504: ("Y", 12.125),
    4505: ("Y", 12.625),
    4506: ("Y", 11.125),
    4507: ("Y", 11.625),
    4508: ("Y", 10.125),
    4509: ("Y", 10.625),
    4510: ("Y", 9.125),
    4512: ("Y", 9.625),
    4513: ("Y", 8.125),
    4514: ("Y", 8.625),
    4515: ("Y", 7.125),
    4516: ("Y", 7.625),
    4518: ("Y", 6.125),
    4519: ("Y", 6.625),
    4520: ("Y", 5.125),
    4521: ("Y", 5.625),
    4523: ("Y", 4.125),
    4524: ("Y", 4.625),
    4525: ("Y", 3.125),
    4526: ("Y", 3.625),
    4527: ("Y", 2.125),
    4528: ("Y", 2.625),
    4529: ("Y", 1.125),
    4530: ("Y", 1.625),
    4531: ("Y", 0.125),
    4532: ("Y", 0.625),
    4539: ("Y", -0.875),
    4540: ("Y", -0.375),
    4541: ("Y", -1.875),
    4542: ("Y", -1.375),
    4543: ("Y", -2.875),
    4544: ("Y", -2.375),
    4545: ("Y", -3.875),
    4546: ("Y", -3.375),
    4547: ("Y", -4.875),
    4548: ("Y", -4.375),
    4550: ("Y", -5.875),
    4551: ("Y", -5.375),
    4552: ("Y", -6.875),
    4553: ("Y", -6.375),
    4555: ("Y", -7.875),
    4556: ("Y", -7.375),
    4557: ("Y", -8.875),
    4558: ("Y", -8.375),
    4559: ("Y", -9.875),
    4561: ("Y", -9.375),
    4562: ("Y", -10.875),
    4563: ("Y", -10.375),
    4564: ("Y", -11.875),
    4565: ("Y", -11.375),
    4566: ("Y", -12.875),
    4567: ("Y", -12.375),
    4568: ("Y", -13.875),
    4569: ("Y", -13.375),
    4570: ("Y", -14.875),
    4571: ("Y", -14.375),
    4572: ("Y", -15.875),
    4573: ("Y", -15.375),
    4574: ("Y", -16.875),
    4575: ("Y", -16.375),
    4576: ("Y", -17.875),
    4577: ("Y", -17.375),
    4578: ("Y", -18.875),
    4579: ("Y", -18.375),
    4580: ("Y", -19.875),
    4581: ("Y", -19.375),
    4582: ("Y", -20.875),
    4584: ("Y", -20.375),
    4585: ("Y", -21.875),
    4586: ("Y", -21.375),
    4587: ("Y", -22.875),
    4588: ("Y", -22.375),
    4590: ("Y", -23.875),
    4591: ("Y", -23.375),
    4592: ("Y", -24.875),
    4593: ("Y", -24.375),
    4595: ("Y", -25.875),
    4596: ("Y", -25.375),
    4597: ("Y", -26.875),
    4598: ("Y", -26.375),
    4599: ("Y", -27.875),
    4600: ("Y", -27.375),
    4601: ("Y", -28.875),
    4602: ("Y", -28.375),
    4603: ("Y", -29.8125),
    4604: ("Y", -29.375),
}

readout_y_min = min([position for signal_type, position in signal_id_readout_mapping.values() if signal_type == "Y"])
readout_y_max = max([position for signal_type, position in signal_id_readout_mapping.values() if signal_type == "Y"])
readout_x_min = min([position for signal_type, position in signal_id_readout_mapping.values() if signal_type == "X"])
readout_x_max = max([position for signal_type, position in signal_id_readout_mapping.values() if signal_type == "X"])


def amplitude_to_color(amplitude, min_amplitude=0, max_amplitude=4095, cmap_name="jet", log_scale=True):
    amplitude = max(amplitude, min_amplitude)

    cmap = plt.get_cmap(cmap_name)

    if log_scale:
        min_amplitude = 1
        amplitude = max(amplitude, min_amplitude)
        log_amplitude = np.log10(amplitude)
        log_min = np.log10(min_amplitude)
        log_max = np.log10(max_amplitude)
        normalized_amplitude = (log_amplitude - log_min) / (log_max - log_min)
    else:
        # Linear scaling
        normalized_amplitude = (amplitude - min_amplitude) / (max_amplitude - min_amplitude)

    # Normalize the value to be between 0 and 1
    normalized_amplitude = np.clip(normalized_amplitude, 0, 1)

    # Convert normalized amplitude to a color using the colormap
    return cmap(normalized_amplitude)


def get_filename_from_prometheus_metrics() -> str | None:
    url = "http://localhost:8080/metrics"
    # try to get it with a timeout of 1 second
    try:
        response = requests.get(url, timeout=1)
        text = response.text
        match = re.search(r'output_root_file_size_mb{filename="([^"]+)"}', text)
        if match:
            return match.group(1)
        else:
            return None

    except requests.exceptions.RequestException:
        return None


def get_event(tree: uproot.TTree, entry: int):
    if entry >= tree.num_entries:
        raise ValueError(
            f"Entry {entry} is out of bounds. Tree has {tree.num_entries} entries."
        )

    events = tree.arrays(entry_start=entry, entry_stop=entry + 1)
    events["signal_values"] = ak.unflatten(events["signal_values"], 512, axis=1)

    signals = ak.Array(
        {"id": events["signal_ids"], "values": events["signal_values"]},
        with_name="Signals",
    )
    events["signals"] = signals

    events = ak.without_field(events, "signal_ids")
    events = ak.without_field(events, "signal_values")

    return events[0]


class EventViewer:
    def __init__(self, _root):
        self.file = None
        self.event_tree = None
        self.run_tree = None
        self.current_entry = 0

        self.root = _root
        self.root.title("Event Viewer")

        self.label = tk.Label(self.root, text="Select a ROOT file to plot signals from")
        self.label.pack(padx=20, pady=5)

        self.file_menu_frame = tk.Frame(self.root, bd=2, relief=tk.FLAT)
        self.file_menu_frame.pack(pady=5, side=tk.TOP)

        self.open_button = tk.Button(
            self.file_menu_frame, text="Open File", command=self.open_file
        )
        self.open_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.reload_file_button = tk.Button(
            self.file_menu_frame, text="Reload", command=self.load_file
        )
        self.reload_file_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.reload_file_button = tk.Button(
            self.file_menu_frame, text="Attach", command=self.attach
        )
        self.reload_file_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.event_frame = tk.Frame(self.root, bd=2, relief=tk.FLAT)
        self.event_frame.pack(pady=5, side=tk.TOP)

        self.prev_button = tk.Button(
            self.event_frame, text="First Event", command=self.first_event
        )
        self.prev_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.prev_button = tk.Button(
            self.event_frame, text="Previous Event", command=self.prev_event
        )
        self.prev_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.entry_textbox = tk.Entry(self.event_frame, width=10)
        self.entry_textbox.pack(side=tk.LEFT, padx=20, pady=5)
        self.entry_textbox.insert(0, "0")

        self.next_button = tk.Button(
            self.event_frame, text="Next Event", command=self.next_event
        )
        self.next_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.prev_button = tk.Button(
            self.event_frame, text="Last Event", command=self.last_event
        )
        self.prev_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.graph_option_frame = tk.Frame(self.root, bd=2)
        self.graph_option_frame.pack(pady=5)

        self.graph_option_waveforms_variable = tk.BooleanVar()
        self.graph_option_waveforms = tk.Checkbutton(self.graph_option_frame, text="Waveforms",
                                                     variable=self.graph_option_waveforms_variable)
        self.graph_option_waveforms.pack(side=tk.LEFT, padx=20, pady=5)
        self.graph_option_waveforms.select()

        self.graph_option_energy_variable = tk.BooleanVar()
        self.graph_option_energy = tk.Checkbutton(self.graph_option_frame, text="Energy",
                                                  variable=self.graph_option_energy_variable)
        self.graph_option_energy.pack(side=tk.LEFT, padx=20, pady=5)

        self.graph_option_readout_variable = tk.BooleanVar()
        self.graph_option_readout = tk.Checkbutton(self.graph_option_frame, text="Readout",
                                                   variable=self.graph_option_readout_variable)
        self.graph_option_readout.select()
        self.graph_option_readout.pack(side=tk.LEFT, padx=20, pady=5)

        self.plot_button = tk.Button(self.graph_option_frame, text="Plot", command=self.plot_graph)
        self.plot_button.pack(side=tk.LEFT, padx=20, pady=5)

        self.graph_frame = tk.Frame(self.root)
        self.graph_frame.pack(fill="both", expand=True)

        # bindings
        self.root.bind("<Left>", lambda _: self.prev_event())
        self.root.bind("<Right>", lambda _: self.next_event())
        self.root.bind("r", lambda _: self.load_file())
        self.root.bind("a", lambda _: self.attach())
        self.root.bind("o", lambda _: self.open_file())
        self.entry_textbox.bind("<Return>", lambda _: self.plot_graph())

        # Initialize the plot area
        self.figure = plt.Figure()

        self.ax_left = None
        self.ax_right = None

        self.canvas = FigureCanvasTkAgg(self.figure, self.graph_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        self.canvas.draw()

        self.label_canvas = tk.Canvas(root, width=150, height=20, bg='white', highlightthickness=0)
        self.label_canvas.place_forget()

        def rgb_to_hex(rgb):
            """Convert an RGB tuple to a hex color string."""
            return '#{:02x}{:02x}{:02x}'.format(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255))

        def on_motion(event):
            # Check if the mouse is over any line
            visible = False
            if self.ax_left is None:
                return

            for line in self.ax_left.lines:
                if line.contains(event)[0]:
                    x, y = event.x, event.y
                    adjusted_y = self.canvas.get_tk_widget().winfo_height() - y
                    line_color = line.get_color()
                    self.label_canvas.delete("all")
                    self.label_canvas.create_rectangle(2, 2, 18, 18, fill=rgb_to_hex(line_color), outline="black")
                    self.label_canvas.create_text(30, 10, anchor='w', text=f'Signal {line.get_label()}', fill='black')
                    self.label_canvas.place(x=x, y=adjusted_y)
                    visible = True
                    break
            if not visible:
                self.label_canvas.place_forget()

        self.canvas.mpl_connect('motion_notify_event', on_motion)

        self.filepath = None

    def attach(self):
        filename = get_filename_from_prometheus_metrics()

        if filename is None:
            messagebox.showerror("Error", "No filename found to attach")
            return

        self.filepath = filename
        self.load_file()

    def load_file(self):
        if self.filepath is None:
            messagebox.showwarning("No File", "You must select a file first")
            return

        self.file = uproot.open(self.filepath)

        try:
            self.event_tree = self.file["events"]
        except KeyError:
            messagebox.showerror(
                "Error",
                f"File {self.filepath} does not contain the 'events' tree. file keys are {self.file.keys()}",
            )
            self.filepath = None
            return

        try:
            self.run_tree = self.file["run"]
        except KeyError:
            messagebox.showerror(
                "Error",
                f"File {self.filepath} does not contain the 'run' tree. file keys are {self.file.keys()}",
            )
            self.filepath = None
            return

        filename_text = self.filepath
        max_file_length = 50
        if len(filename_text) > max_file_length:
            filename_text = filename_text.split("/")[-1]
        if len(filename_text) > max_file_length:
            filename_text = filename_text[:max_file_length] + "..."

        self.label.config(
            text=f"{filename_text} - {self.event_tree.num_entries} entries"
        )

        self.plot_graph()

    def open_file(self):
        self.filepath = filedialog.askopenfilename(filetypes=[("ROOT files", "*.root")])
        if not self.filepath:
            return

        self.load_file()

    def check_file(self) -> bool:
        if self.filepath is None or self.event_tree is None or self.run_tree is None:
            messagebox.showwarning("No File", "Please select a file first!")
            return False
        return True

    def plot_readout(self):
        entry = int(self.entry_textbox.get())
        self.current_entry = entry

        event = get_event(self.event_tree, entry)

        if self.ax_right is None:
            self.ax_right = self.figure.add_subplot(122)

        self.ax_right.clear()  # Clear the previous plot

        # get min value of all signals
        min_value = np.min([np.min(values) for values in event.signals.values])

        line_width = 4
        for signal_id, values in zip(event.signals.id, event.signals.values):
            signal_id = int(signal_id)
            if signal_id not in signal_id_readout_mapping:
                # print(f"Signal {signal_id} not found in mapping.")
                continue
            signal_type, position = signal_id_readout_mapping[signal_id]
            is_x_signal = signal_type == "X"
            amplitude = np.max(values)  # amplitude goes from 0 to 4095
            amplitude = amplitude - min_value
            line_color = amplitude_to_color(amplitude)
            alpha = amplitude / 4095
            alpha *= 5.0 / 3.0
            alpha = np.clip(alpha, 0.1, 1.0)
            if is_x_signal:
                self.ax_right.plot([readout_y_min, readout_y_max], [position, position], color=line_color,
                                   alpha=alpha,
                                   linewidth=line_width)
            else:
                self.ax_right.plot([position, position], [readout_x_min, readout_x_max], color=line_color,
                                   alpha=alpha,
                                   linewidth=line_width)

        self.ax_right.set_xlabel("X (mm)")
        self.ax_right.set_ylabel("Y (mm)")
        self.ax_right.set_aspect("equal")
        self.ax_right.set_xlim(readout_x_min - 1.0, readout_x_max + 1.0)
        self.ax_right.set_ylim(readout_y_min - 1.0, readout_y_max + 1.0)
        self.ax_right.set_title(
            f"Event {entry} - Number of signals: {len(event.signals.id)}"
        )

        self.canvas.draw()

    def plot_waveforms(self):
        entry = int(self.entry_textbox.get())
        self.current_entry = entry

        event = get_event(self.event_tree, entry)

        if self.ax_left is None:
            self.ax_left = self.figure.add_subplot(121)

        self.ax_left.clear()  # Clear the previous plot
        for signal_id, values in zip(event.signals.id, event.signals.values):
            self.ax_left.plot(values, label=f"ID {signal_id}", alpha=0.75, linewidth=2)

        n_signals_showing = len(self.ax_left.lines)

        self.ax_left.set_xlabel("Time bins")
        self.ax_left.set_ylabel("ADC")

        self.ax_left.set_title(
            f"Event {entry} - Number of signals: {len(event.signals.id)}"
        )

        self.ax_left.set_xlim(0, 512)
        self.ax_left.set_xticks(range(0, 512 + 1, 64))
        self.ax_left.set_xticks(range(0, 512 + 1, 16), minor=True)
        self.ax_left.set_aspect("auto")

        if n_signals_showing <= 10:
            self.ax_left.legend(loc="upper right")

        self.canvas.draw()

    def plot_graph(self):
        if not self.check_file():
            return

        try:
            if not self.graph_option_waveforms_variable.get() and not self.graph_option_energy_variable.get() and not self.graph_option_readout_variable.get():
                self.graph_option_waveforms.select()

            if self.graph_option_waveforms_variable.get():
                self.plot_waveforms()

            if self.graph_option_energy_variable.get():
                self.plot_energy()

            if self.graph_option_readout_variable.get():
                self.plot_readout()

        except ValueError as e:
            messagebox.showerror("Error", f"Invalid entry: {str(e)}")
        except Exception as e:
            messagebox.showerror("Error", f"An error occurred while plotting: {str(e)}")

    def prev_event(self):
        if not self.check_file():
            return

        if self.current_entry > 0:
            self.current_entry -= 1
            self.entry_textbox.delete(0, tk.END)
            self.entry_textbox.insert(0, str(self.current_entry))
            self.plot_graph()

    def next_event(self):
        if not self.check_file():
            return

        if self.current_entry == self.event_tree.num_entries - 1:
            self.load_file()

        if self.event_tree and self.current_entry < self.event_tree.num_entries - 1:
            self.current_entry += 1
            self.entry_textbox.delete(0, tk.END)
            self.entry_textbox.insert(0, str(self.current_entry))
            self.plot_graph()

    def first_event(self):
        if not self.check_file():
            return

        self.current_entry = 0
        self.entry_textbox.delete(0, tk.END)
        self.entry_textbox.insert(0, str(self.current_entry))
        self.plot_graph()

    def last_event(self):
        if not self.check_file():
            return

        self.load_file()  # Reload the file in case it's been updated
        self.current_entry = self.event_tree.num_entries - 1
        self.entry_textbox.delete(0, tk.END)
        self.entry_textbox.insert(0, str(self.current_entry))
        self.plot_graph()


if __name__ == "__main__":
    root = tk.Tk()
    app = EventViewer(root)
    root.mainloop()
