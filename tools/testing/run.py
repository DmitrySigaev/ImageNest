import os
import subprocess
import shutil
import time
import StringIO

from indigo.indigo_renderer import *
from indigo.indigo import *

out_dir = "results"

tmp_dir = "tmp"

def createDir(dir):
    if not os.path.exists(dir):
        os.makedirs(dir)

createDir(tmp_dir) # for output
createDir(os.path.join(tmp_dir, "output/segs")) # for debug output
    
indigo = Indigo()
renderer = IndigoRenderer(indigo)

indigo.setOption("render-output-format", "png");
indigo.setOption("render-background-color", "255, 255, 255"); 
indigo.setOption("render-coloring", "true");
indigo.setOption("treat-x-as-pseudoatom", "true")
indigo.setOption("treat-x-as-pseudoatom", "true")
indigo.setOption("ignore-stereochemistry-errors", "true")

versions = sorted(os.listdir("versions"))

class ItemVersion:
    def __init__ (self, base):
        self.base = base
        self.molfile = base + '.mol'
        self.timefile = base + '.time'
        self.imgfile = base + '.png'
        
    def setFailed (self):
        self.base = None
        
    def getMolFile (self):
        return self.molfile
    def getTimeFile (self):
        return self.timefile
    def getRenderedFile (self):
        img_name = self.imgfile
        if os.path.exists(img_name):
            return img_name
        
        # Render the image
        try:
            m = indigo.loadMoleculeFromFile(self.getMolFile())
            renderer.renderToFile(m, img_name)
        except IndigoException:
            return None
        
        return img_name
            
    def setTime(self, value):
        f = open(self.getTimeFile(), "w")
        f.write("%0.3f" % (value))
        f.close()
        
    def getTime(self):
        f = open(self.getTimeFile())
        value = f.read()
        f.close()
        return float(value)
        
    def exists (self):
        return os.path.exists(self.getMolFile())

class Item:
    def __init__ (self, photo_full):
        self.dirname, self.photo = os.path.split(photo_full)
        print(self.dirname, " : ", self.photo)
        basename_group = self.photo.split('.')[0]
        
        ref_mol_name = os.path.join(self.dirname, basename_group + ".mol")
        self.reference = None
        if os.path.exists(ref_mol_name):
            self.reference = ref_mol_name
        else:
            self.reference = None
        
        self.versions = dict()
        
    def getImageFileName (self):
        return os.path.join(self.dirname, self.photo)
        
    def processVersion (self, version):
        vname, vext = os.path.splitext(version)
        mol_dest_dir = os.path.join(out_dir, vname, self.dirname)
        createDir(mol_dest_dir)
        
        mol_dest = os.path.join(mol_dest_dir, self.photo + "." + vname)
        
        item_version = ItemVersion(mol_dest)
        self.versions[version] = item_version
        if item_version.exists():
            return

        print("*****")
        print("* Version: " + version)
        print("* Image: " + os.path.join(self.dirname, self.photo))
            
        full_version = os.path.join("versions", version)
        abs_version = os.path.abspath(full_version)
        abs_img = os.path.abspath(os.path.join(self.dirname, self.photo))
        tl_exe = os.path.abspath("TimeLimit.exe")
        
        os.chdir(tmp_dir)
        if os.path.exists("molecule.mol"):
            os.remove("molecule.mol")
        time_before = time.clock()
        ret = subprocess.call([tl_exe, "30", abs_version, abs_img]) 
        time_after = time.clock()
        
        os.chdir("..")
        time_val = time_after - time_before
        item_version.setTime(time_val)
        if time_val > 16.0:
            print 'timelimit exceeded'
            raise
        
        if ret != 0:
            item_version.setFailed()
            return
        
        # Copy
        mol_created = os.path.join(tmp_dir, "molecule.mol")
        if not os.path.exists(mol_created):
            item_version.setFailed()
            return
        shutil.copy(mol_created, item_version.getMolFile())

class Group:
    def __init__ (self, root):
        self.name = os.path.split(root)[1]
        self.items = []
        self.subgroups = []
        
    def processVersion (self, version):
        for item in self.items:
            item.processVersion(version)
        for group in self.subgroups:
            group.processVersion(version)

def collectGroup (root):
    g = Group(root)
    for item in os.listdir(root):
        name, ext = os.path.splitext(item)
        if ext.lower() == '.mol':
            continue
        rootitem = os.path.join(root, item)
        if os.path.isdir(rootitem):
            g.subgroups.append(collectGroup(rootitem))
        else:
            g.items.append(Item(rootitem))
    return g
    
groups = collectGroup("images")    
for version in versions:
    groups.processVersion(version)

class VersionScore:
    def __init__ (self):
        self.score_sum = 0
        self.time_sum = 0
        self.count = 0
        self.good_count = 0
    def add (self, other):
        self.score_sum += other.score_sum
        self.time_sum += other.time_sum
        self.count += other.count
        self.good_count += other.good_count

class VersionsScore:
    def __init__ (self):
        self.versions = dict()
    def __getitem__ (self, index):
        if index not in self.versions:
            self.versions[index] = VersionScore()
        return self.versions[index]
    def add (self, other):
        for version in other.versions:
            self[version].add(other[version])
        
    
# generate report
report_data = StringIO.StringIO()
  
def measureSimilarity (m1, m2):
    if not m1 or not m2:
        return None
        
    sim = indigo.similarity(m1, m2, "tanimoto")
    
    n1 = m1.countAtoms()
    n2 = m2.countAtoms()
    if n1 + n2 != 0:
        adiff = 1 - (float(abs(n1 - n2)) / (n1 + n2)) ** 0.9
    else:
        adiff = 1
    
    s1 = 0
    s2 = 0
    for b in m1.iterateBonds():
        if b.bondStereo() == Indigo.UP or b.bondStereo() == Indigo.DOWN:
            s1 += 1
    for b in m2.iterateBonds():
        if b.bondStereo() == Indigo.UP or b.bondStereo() == Indigo.DOWN:
            s2 += 1
    if s1 + s2 != 0:
        cdiff = 1 - 0.05 * (float(abs(s1 - s2)) / (s1 + s2)) ** 0.6
    else:
        cdiff = 1
    
    return sim * adiff * cdiff * 100
  
def getExperimentClass (sim, sim_values, scores):
    cls = ""
    sim_value = 0
    if sim != None:
        sim_value = sim
        
    if len(sim_values) > 0:
        if sim_value < max(sim_values) - 10:
            cls = "regression"
        if sim_value < sim_values[-1] - 10:
            cls = "newregression"
    if sim_value > 94.5:
        cls = "correct"
        scores.good_count += 1
    if cls == "" and len(sim_values) > 0:
        if sim_value > sim_values[-1] + 15:
            cls = "improvement"
            
    if len(sim_values) > 0 and sim_value > sim_values[-1]:
        cls += " better"
        
    sim_values.append(sim_value)
    scores.score_sum += sim_value
    scores.count += 1
        
    return cls
    
  
def generateGroupReport (g, level):
    if level >= 0:
        report_data.write("<tr><td class='level%d group'>%s</td></tr>\n" % (level, g.name))
    # process all subgroups
    stat = VersionsScore()
    for subgroup in g.subgroups:
        subdata = generateGroupReport(subgroup, level + 1)
        stat.add(subdata)
     
    for item in g.items:
        report_data.write("<tr><td class='level%d'><a href='%s'>%s</a></td>\n" % 
            (level + 1, item.getImageFileName(), item.photo))
        ref_mol = None
        if item.reference:
            try:
                ref_mol = indigo.loadMoleculeFromFile(item.reference)
            except IndigoException:
                pass
                
        prev_molecule = None
        sim_values = []
        for version in versions:
            print("*****")
            print("* Version: " + version)
            print("* Image: " + item.getImageFileName())
            item_version = item.versions[version]
            
            time_val = item_version.getTime()
            stat[version].time_sum += time_val
            
            item_mol = None
            try:
                item_mol = indigo.loadMoleculeFromFile(item_version.getMolFile())
                cur_molecule = item_mol.molfile()
            except IndigoException:
                cur_molecule = ""
                pass
            sim = measureSimilarity(ref_mol, item_mol)
            print(sim)

            if sim != None:
                sim_value = "%0.0f" % sim
            else:
                sim_value = "?"
            
            cls = getExperimentClass(sim, sim_values, stat[version])
            
            if cls == "" and prev_molecule == cur_molecule:
                cls = "same"
            
            prev_molecule = cur_molecule
            
            rendered = item.versions[version].getRenderedFile()
            if rendered:
                text = "<a href='%s'>%s</a>" % (rendered, sim_value)
            else:
                text = "%s ?" % (sim_value)
            cls_expr = ""
            if cls != "":
                cls_expr = "class='%s'" % (cls)
                
            report_data.write("<td %s>%s</td>\n" % (cls_expr, text))
            
        report_data.write("</tr>\n")
    # print statistics
    report_data.write("<tr><td class='level%d stat'>avg. score</td>\n" % (level + 1))
    for version in versions:
        version_stat = stat[version]
        if version_stat.count != 0:
            avg_score = "%0.2f" % (version_stat.score_sum / version_stat.count)
        else:
            avg_score = "?"
        
        report_data.write("<td class='stat'>%s</td>\n" % (avg_score))
    report_data.write("</tr>\n")
    report_data.write("<tr><td class='level%d stat'>avg. time</td>\n" % (level + 1))
    for version in versions:
        version_stat = stat[version]
        if version_stat.count != 0:
            avg_time = "%0.2f" % (version_stat.time_sum / version_stat.count)
        else:
            avg_time = "?"
        
        report_data.write("<td class='stat'>%s</td>\n" % (avg_time))
    report_data.write("</tr>\n")
    report_data.write("<tr><td class='level%d stat'># correct</td>\n" % (level + 1))
    for version in versions:
        version_stat = stat[version]
        report_data.write("<td class='stat'>%d</td>\n" % (version_stat.good_count))
    report_data.write("</tr>\n")

    if level == -1:
        open("score.txt", "w").write("%s" % (avg_score))
    
    return stat
    
    
report_data.write("<table>")    
report_data.write("<tr><td class='level0 group'>Image</td>")
for version in versions:
    name, ext = os.path.splitext(version)
    report_data.write("<td class='group'>%s</td>" % (name))
report_data.write("</tr>")

generateGroupReport(groups, -1)

report_data.write("</table>")    
    
report_template = open("template.html").read()
report = report_template.replace("SCORES", report_data.getvalue())
open("report.html", "w").write(report)