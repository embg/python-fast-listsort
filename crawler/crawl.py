import sys
assert(sys.version_info >= (3,5))

import re
from requests import get
import subprocess
def run(s):
    return subprocess.run(s,
                          shell=True,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL)
    
directory = open('list.html').read() #download list from Browse Packages ->
                                     #                   Python 3.4 -> Show All

custom = no_custom = failiures = 0
for (package_url, package_name) in \
    re.findall('(https://pypi\.python\.org/pypi/([^/]+)/)', directory):
    print(custom+no_custom+failiures,
          custom,
          no_custom,
          failiures)

    try:
        package_page = get(package_url).text
        (download_url,file_type) = re.search('<a href="(.+)">.+(\.tar\.gz|\.zip)</a>',
                                             package_page).groups()
        print(package_name)
        archive = open('archive', 'wb')
        archive.write(get(download_url).content)
        archive.close()

        run('rm -r package_code')
        run('mkdir package_code')
        
        if file_type == '.tar.gz':
            run('tar -xzf archive -C package_code')
        if file_type == '.zip':
            run('unzip archive -d package_code')
        return_code = run('grep -Er "def __(le|lt|ge|gt)__" ./package_code').returncode
        
        if return_code == 0:
            custom += 1
        elif return_code == 1:
            no_custom += 1
        else:
            failiures += 1

    except (KeyboardInterrupt, SystemExit):
        raise
    except Exception as exception: #for when there is no .tar.gz or .zip on PyPI
                                   #or (rarely) when the connection is dropped
        print("FAILIURE:", type(exception).__name__)
        failiures += 1
        
print("""
Packages that define custom compare operators: %i
Packages that don't define custom operators: %i
Packages that didn't have source on PyPI: %i
""" % (custom, no_custom, failiures))
