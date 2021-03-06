#!/bin/bash
# remote-fetch.sh - Update git repository URL if needed
#    Copyright (C) 2018  Jeremy Lincicome (W0JRL)
#    https://jlappliedtechnologies.com  admin@jlappliedtechnologies.com

#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Script Start
cd /usr/src/utils
echo "Checking repository URL..."
if [ "$(git config --get remote.origin.url | grep -ic "AllStar-raspbian")" == "1" ]; then
  echo "Updating repository URL and data..."
  git remote set-url origin https://github.com/w0jrl/raslink.git
  git pull &>/dev/null
  echo "Done"
else
  echo "The repository URL is up-to-date; Skipping."
fi
exit 0
