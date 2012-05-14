#!/usr/bin/python
from PyQt4 import QtCore, QtGui
from PyQt4.Qt import *

try:
  _fromUtf8 = QtCore.QString.fromUtf8
except AttributeError:
  _fromUtf8 = lambda s: s


class OptionsGUI(QtGui.QDialog):
  def __init__(self, main_data, single_item, incoming_data, win_parent=None):
    QtGui.QDialog.__init__(self, win_parent)
    self.main_data = main_data
    self.single_item = single_item
    self.original_table_data = {}
#    self.widget_list = []
    self.button_list =[]
#    self.layout_group = []
    self.layout_buttons = []
    self.incoming_data = incoming_data
    self.initUI()
    self.changed_cells = {}

  def initUI(self):
    # Init main window
    self.main_ui = QtGui.QWidget(self)
    self.main_ui.setObjectName(_fromUtf8("Dialog"))

    # Set layout
    self.layoutV = QtGui.QVBoxLayout(self)

    # build main window
    self.init_menu(self.layoutV)

    # combine it all together
    self.table_widget = QtGui.QTableWidget()
    self.layoutV.addWidget(self.table_widget)
    # self.main_ui.setLayout(self.layoutV)

    self.drop_menu.setCurrentIndex(-1)
    # print self.incoming_data
    if self.incoming_data:
      if 'type' in self.incoming_data:
        self.drop_menu.setCurrentIndex(self.drop_menu.findText(self.incoming_data['type']))
      else:
        found_index = self.drop_menu.findText(self.incoming_data['Name'])
        if found_index != -1:
          self.drop_menu.setCurrentIndex(found_index)
        else:
          self.drop_menu.setCurrentIndex(0) #Hack until we figure out something better
      self.table_widget.cellChanged.connect(self.cellChanged)
      self.fillTableWithData(self.incoming_data)
      self.table_widget.cellChanged.disconnect(self.cellChanged)

    self.resize(700,500)

  ### Takes a dictionary containing name value pairs
  def fillTableWithData(self, the_data):
    for name,value in the_data.items():
      for i in xrange(0,self.total_rows):
        row_name = str(self.table_widget.item(i,0).text())

        if row_name == name:
          item = self.table_widget.item(i,1)
          item.setText(value)

  def tableToDict(self, only_not_in_original = False):
    the_data = {}
    
    for i in xrange(0,self.total_rows):
      param_name = str(self.table_widget.item(i,0).text())
      param_value = str(self.table_widget.item(i,1).text())
      
      if not param_name in self.original_table_data or not self.original_table_data[param_name] == param_value: #If we changed it - definitely include it
          the_data[param_name] = param_value
      else:
        if not only_not_in_original: # If we want stuff other than what we changed
          if param_name == 'parent_params' or param_name == 'type':  #Pass through type and parent_params even if we didn't change them
             the_data[param_name] = param_value
          else:
            if param_name in self.param_is_required and self.param_is_required[param_name]: #Pass through any 'required' parameters
              the_data[param_name] = param_value
              
    return the_data
    

  def init_menu(self, layout):
    self.drop_menu = QtGui.QComboBox()
    for item in self.main_data:
      self.drop_menu.addItem(item['name'].split('/').pop())
#    self.drop_menu.activated[str].connect(self.item_clicked)
    self.drop_menu.currentIndexChanged[str].connect(self.item_clicked)
    layout.addWidget(self.drop_menu)

  def click_add(self):
    #print 'add'
    self.table_data = self.tableToDict()
    self.accept()
    return

  def result(self):
    return self.table_data

  def click_cancel(self):
    #rint 'cancel'
    self.reject()

  def item_clicked(self, item):
    #print "item_clicked"
    # Hide previous widgets (you can not delete them apparently)
    # TODO: theres gotta be a way to handle destroying widgets no
    # longer needed.
    for button in self.button_list:
      button.hide()

    try:
      self.table_widget.cellChanged.disconnect(self.cellChanged)
    except:
      pass

    saved_data = {}
    # Save off the current contents to try to restore it after swapping out the params
    if self.original_table_data: #This will only have some length after the first time through
      saved_data = self.tableToDict(True) # Pass true so we only save off stuff the user has entered

    # Build the Table
    the_table_data = []

    # Save off thie original data from the dump so we can compare later
    self.original_table_data = {}
    self.param_is_required = {}

    the_table_data.append({'name':'Name','default':'','description':'Name you want to give to this object','required':True})
    
    for new_text in self.main_data:
      if new_text['name'].split('/').pop() == item:
        #the_table_data.append({'name':'type','default':new_text['name'].split('/').pop(),'description':'The object type','required':True})
        for param in new_text['parameters']:
          self.original_table_data[param['name']] = param['default']
          if param['name'] == 'type':
            param['default'] = new_text['name'].split('/').pop()
          the_table_data.append(param)
          self.param_is_required[param['name']] = param['required']
        break #- can't break here because there might be more

    self.total_rows = len(the_table_data)
    self.table_widget.setRowCount(self.total_rows)
    self.table_widget.setColumnCount(3)
    self.table_widget.setHorizontalHeaderItem(0, QtGui.QTableWidgetItem('Name'))
    self.table_widget.setHorizontalHeaderItem(1, QtGui.QTableWidgetItem('Value'))
    self.table_widget.setHorizontalHeaderItem(2, QtGui.QTableWidgetItem('Description'))
    self.table_widget.verticalHeader().setVisible(False)

    has_parent_params_set = False

    #Search for 'parent_params' parameter so we know what to do with the Name
    for param in the_table_data:
      if param['name'] == 'parent_params':
        has_parent_params_set = True
        break

    row = 0
    for param in the_table_data:
      # Populate table with data:
      name_item = QtGui.QTableWidgetItem(param['name'])

      value = ''
      
      if not param['required'] or param['name'] == 'type':
        value = param['default']

      if param['required']:
        color = QtGui.QColor()
#        color.setNamedColor("red")
        color.setRgb(255,204,153)
        name_item.setBackgroundColor(color)

      if has_parent_params_set and param['name'] == 'Name':
        value = 'ParentParams'
        
      value_item = QtGui.QTableWidgetItem(value)

      doc_item = QtGui.QTableWidgetItem(param['description'])
      
      name_item.setFlags(QtCore.Qt.ItemIsEnabled)
      doc_item.setFlags(QtCore.Qt.ItemIsEnabled)

      if param['name'] == 'type' or param['name'] == 'parent_params' or (has_parent_params_set and param['name'] == 'Name'):
        value_item.setFlags(QtCore.Qt.NoItemFlags)

      self.table_widget.setItem(row, 0, name_item)
      self.table_widget.setItem(row, 1, value_item)
      self.table_widget.setItem(row, 2, doc_item)
      row += 1

    self.table_widget.resizeColumnsToContents()

    # Build the Add and Cancel buttons
    if self.incoming_data and len(self.incoming_data):
      self.button_list.append(QtGui.QPushButton("Apply"))
    else:
      self.button_list.append(QtGui.QPushButton("Add"))
      
    self.button_list.append(QtGui.QPushButton("Cancel"))
    QtCore.QObject.connect(self.button_list[len(self.button_list) - 2], QtCore.SIGNAL("clicked()"), self.click_add)
    QtCore.QObject.connect(self.button_list[len(self.button_list) - 1], QtCore.SIGNAL("clicked()"), self.click_cancel)
    self.layout_buttons.append(QtGui.QHBoxLayout())
    for button in self.button_list:
      self.layout_buttons[len(self.layout_buttons) - 1].addWidget(button)
    self.layoutV.addLayout(self.layout_buttons[len(self.layout_buttons) - 1])

    # Try to restore saved data
    if len(saved_data):
      self.fillTableWithData(saved_data)
    
    self.table_widget.cellChanged.connect(self.cellChanged)
    
  def cellChanged(self, row, col):
    pass
    #print "Changed!"