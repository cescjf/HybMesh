#!/usr/bin/env python
"""
Console interface for HybMesh. Possible arguments:
-v -- print version and exit
-u -- check for updates and exit
------------------------------------------------
-x fn.hmp [-sgrid gname fmt fn] [-sproj fn]
Execute command flow from 'hmp' file and saves resulting data.

Options:

-sgrid gname fmt fn -- export resulting grid with called gname to
file fn using format fmt. Can be called multiple times.
    Formats:
        vtk - vtk format
        hmg - native HybMesh format
        msh - fluent mesh format

-sproj fn -- save project file after execution to file fn

Example:

> HybMesh -x state1.hmp -sgrid Grid1 vtk grid1.vtk -sproj final.hmp
    1) loads data from stat1.hmp
    2) executes command flow
    3) exports Grid1 to vtk format
    4) saves finalized project to final.hmp
"""
import sys
import xml.etree.ElementTree as ET
import progdata
import com.flow
import gdata.framework
from imex import readxml, writexml
import basic.interf


def main():
    if len(sys.argv) == 1 or sys.argv[1] in ['help', 'h', '-help', '-h']:
        print __doc__
        quit()

    if len(sys.argv) == 2:
        if sys.argv[1] == '-v':
            print progdata.program_version()
            quit()
        if sys.argv[1] == '-u':
            r = progdata.check_for_updates()
            print 'Current version: %s' % r[0]
            if r[2] is None:
                print 'Failed to check for latest version'
            elif r[2] != 1:
                print 'No updates are availible'
            else:
                print 'Latest version: %s is availible at %s' % (
                            r[1], progdata.project_url())
            quit()

    # execution
    if '-x' in sys.argv:
        fn = sys.argv[sys.argv.index('-x') + 1]
        # load flow and state
        root = ET.parse(fn).getroot()
        flow_nodes = root.findall('.//FLOW')
        if len(flow_nodes) == 0:
            raise Exception('No proper data in %s' % fn)
        flow_node = flow_nodes[0]
        f = com.flow.CommandFlow()
        readxml.load_command_flow(f, flow_node)
        state_node = flow_node.find('STATE')
        if state_node is not None:
            data = com.framework.Framework
            readxml.load_framework_state(data, state_node)
        else:
            data = gdata.framework.Framework()
        f.set_receiver(data)
        f.set_interface(basic.interf.ConsoleInterface())

        # run till end
        f.exec_all()

        # save current state
        if '-sproj' in sys.argv:
            fn = sys.argv[sys.argv.index('-sproj') + 1]
            writexml.write_flow_and_framework_to_file(f, data, fn)

        # export grid
        for i, op in enumerate(sys.argv):
            if op == '-sgrid':
                gname = sys.argv[i + 1]
                # fmt = sys.argv[i + 2]
                fn = sys.argv[i + 3]
                # grid = data.get_grid(name=gname)
                # imex.export_grid(grid, fmt, fn)
                print '%s save to %s' % (gname, fn)

        print "DONE"
        quit()

    print 'Invalid option string. See -help'


if __name__ == '__main__':
    main()