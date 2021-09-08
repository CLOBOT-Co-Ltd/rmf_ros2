# Install Clober RMF

clone the CLOBER RMF packages
```bash
mkdir -p ~/clober_rmf_ws/src
cd ~/clober_rmf_ws
wget https://raw.githubusercontent.com/CLOBOT-Co-Ltd/rmf_ros2/clober-dev/rmf.repos
vcs import src < rmf.repos

cd ~/clober_rmf_ws/src
git clone https://github.com/CLOBOT-Co-Ltd/clober_rmf.git -b dev
```