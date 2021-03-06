import os, sys

control_in_filename = ["w2", "w2s", "w3", "w3s", "w4", "w4s", "x10", "x10s"]
start_ind = 0
end_ind = -1
single_horizon = [0, 5]
noise_thresh = [1.0, 1.2, 1.4, 1.6, 1.8, 2.0]

for control_in in control_in_filename:
  for horizon in single_horizon:
    for noise in noise_thresh:
      for i in range(0, 10):
        run_command = "%s %s %s %s %d %d %d %f" % ("./runExperiment", control_in + "_" + str(start_ind) + "_" + str(end_ind) + "_" + str(horizon) + "_" + str(noise) + "_exp" + str(i), control_in, control_in + "_world", start_ind, end_ind, horizon, noise)
        print run_command
        os.system(run_command)
