# Install Clober RMF

clone the CLOBER RMF packages
```bash
mkdir -p ~/clober_rmf_ws/src
cd ~/clober_rmf_ws
wget https://raw.githubusercontent.com/CLOBOT-Co-Ltd/rmf_ros2/clober-dev/clober_rmf.repos
vcs import src < rmf.repos

cd ~/clober_rmf_ws/src
git clone https://github.com/CLOBOT-Co-Ltd/clober_rmf.git -b dev
```

# Build for CLOBER

Add environment variabels `CLOBER_RMF`

If you want to run the CLOBER RMF, execute this command.
```bash
export CLOBER_RMF=1

cd ~/clober_rmf_ws/
colcon build
```

If you want to run the Original RMF, don't declare the `CLOBER_RMF`.