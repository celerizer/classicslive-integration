#include <QMessageBox>

#include "cle_memory_inspector.h"
#include "cle_common.h"
#include "cle_result_table_normal.h"
#include "cle_result_table_pointer.h"

extern "C" 
{
   #include "../cl_common.h"
   #include "../cl_main.h"
}

void CleMemoryInspector::rebuildLayout()
{
   m_Layout = new QGridLayout(this);

   /* Initialize window layout */
   m_Layout->addWidget(m_Tabs,            0, 0, 1, 2);
   m_Layout->addWidget(m_SizeDropdown,    1, 0);
   m_Layout->addWidget(m_NewButton,       1, 1);
   m_Layout->addWidget(m_CompareDropdown, 2, 0);
   m_Layout->addWidget(m_TextEntry,       2, 1);
   m_Layout->addWidget(m_SearchButton,    3, 0, 1, 2);
   m_Layout->addWidget(m_TableStack,      4, 0, 2, 2);
   m_Layout->addWidget(m_HexWidget,       6, 0, 2, 2);
   m_Layout->addWidget(m_Slider,          6, 2, 1, 1);
   setLayout(m_Layout);
}

CleMemoryInspector::CleMemoryInspector()
{
   /* Initialize basic elements */
   m_NewButton = new QPushButton(tr("&New Search"));
   connect(m_NewButton, SIGNAL(clicked()), 
      this, SLOT(onClickNew()));

   m_SearchButton = new QPushButton(tr("&Search"));
   m_SearchButton->setAutoDefault(true);
   connect(m_SearchButton, SIGNAL(clicked()), 
      this, SLOT(onClickSearch()));

   /* Initialize dropdown boxes */
   m_CompareDropdown = new QComboBox();
   m_CompareDropdown->addItem(tr("are equal to..."),      CLE_CMPTYPE_EQUAL);
   m_CompareDropdown->addItem(tr("are greater than..."),  CLE_CMPTYPE_GREATER);
   m_CompareDropdown->addItem(tr("are less than..."),     CLE_CMPTYPE_LESS);
   m_CompareDropdown->addItem(tr("are not equal to..."),  CLE_CMPTYPE_NOT_EQUAL);
   m_CompareDropdown->addItem(tr("have increased by..."), CLE_CMPTYPE_INCREASED);
   m_CompareDropdown->addItem(tr("have decreased by..."), CLE_CMPTYPE_DECREASED);
   m_CompareDropdown->addItem(tr("are above address..."), CLE_CMPTYPE_ABOVE);
   m_CompareDropdown->addItem(tr("are below address..."), CLE_CMPTYPE_BELOW);
   connect(m_CompareDropdown, SIGNAL(activated(int)), 
      this, SLOT(onChangeCompareType()));

   m_SizeDropdown = new QComboBox();
   m_SizeDropdown->addItem(tr("1-byte values"), CL_MEMTYPE_UINT8);
   m_SizeDropdown->addItem(tr("2-byte values"), CL_MEMTYPE_UINT16);
   m_SizeDropdown->addItem(tr("4-byte values"), CL_MEMTYPE_INT32);
   m_SizeDropdown->addItem(tr("float values"),  CL_MEMTYPE_FLOAT);
   connect(m_SizeDropdown, SIGNAL(activated(int)),
      this, SLOT(onChangeSizeType()));

   /* Initialize text entry box for comparison value */
   m_TextEntry = new QLineEdit();
   connect(m_TextEntry, SIGNAL(returnPressed()), 
      m_SearchButton, SIGNAL(clicked()));

   /* Initialize tab view for switching between searches */
   m_Tabs = new QTabBar();
   m_Tabs->setContextMenuPolicy(Qt::CustomContextMenu);
   m_Tabs->setExpanding(false);
   m_Tabs->addTab(tr("Search 1"));
   m_Tabs->addTab(tr("+"));
   connect(m_Tabs, SIGNAL(currentChanged(int)), 
      this, SLOT(onChangeTab()));
   connect(m_Tabs, SIGNAL(customContextMenuRequested(const QPoint&)), 
      this, SLOT(onRightClickTabs(const QPoint&)));

   /* Initialize scrollbar */
   m_Slider = new QSlider(this);
   m_Slider->setInvertedAppearance(true);
   m_Slider->setTickInterval(1024 * 1024); //1MB
   m_Slider->setTickPosition(QSlider::TicksRight);
   connect(m_Slider, SIGNAL(sliderMoved(int)),
      this, SLOT(onChangeScrollbar(int)));

   /* Initialize hex value view widget */
   m_HexWidget = new CleHexWidget(this, 1);
   m_BufferPrevious = (uint8_t*)malloc(256);
   m_BufferCurrent  = (uint8_t*)malloc(256);
   connect(m_HexWidget, SIGNAL(offsetEdited(cl_addr_t)), 
      this, SLOT(onAddressChanged(cl_addr_t)));
   connect(m_HexWidget, SIGNAL(valueEdited(cl_addr_t, uint8_t)),
      this, SLOT(onHexWidgetValueEdited(cl_addr_t, uint8_t)));
   connect(m_HexWidget, SIGNAL(requestAddMemoryNote(cl_addr_t)),
      this, SLOT(requestAddMemoryNote(cl_addr_t)));
   connect(m_HexWidget, SIGNAL(requestPointerSearch(cl_addr_t)),
      this, SLOT(requestPointerSearch(cl_addr_t)));
   m_HexWidget->setByteSwapEnabled(memory.regions[0].endianness == CL_ENDIAN_BIG);
   m_HexWidget->setOffset(memory.regions[0].base_guest);
   m_HexWidget->setRange(memory.regions[0].base_guest, memory.regions[0].base_guest + memory.regions[0].size + 1);

   /* Initialize timer for updating search rows */
   m_UpdateTimer = new QTimer(this);
   connect(m_UpdateTimer, SIGNAL(timeout()), this, SLOT(run()));
   m_UpdateTimer->start(100);

   memset(m_Searches, 0, sizeof(m_Searches));
   m_Searches[0] = new CleResultTableNormal(this);
   m_CurrentSearch = m_Searches[0];

   m_TableStack = new QStackedWidget(this);
   m_TableStack->addWidget(m_CurrentSearch->getTable());

   rebuildLayout();
   setWindowTitle(tr("Live Editor"));

   /* Initialize other variables */
   m_AddressOffset = 0;
   m_CurrentMembank = &memory.regions[0];
   m_TabCount = 1;
   m_MemoryNoteSubmit = NULL;

   onChangeCompareType();
}

uint8_t CleMemoryInspector::getCurrentCompareType(void)
{
   return m_CompareDropdown->itemData(m_CompareDropdown->currentIndex()).toUInt();
}

uint8_t CleMemoryInspector::getCurrentSizeType(void)
{
   return m_SizeDropdown->itemData(m_SizeDropdown->currentIndex()).toUInt();
}

void CleMemoryInspector::onAddressChanged(cl_addr_t address)
{
   cl_memory_region_t *new_bank = cl_find_memory_region(address);

   if (!new_bank)
      return;
   else
   {
      if (m_CurrentMembank != new_bank)
      {
         m_HexWidget->setRange(new_bank->base_guest, new_bank->base_guest + new_bank->size + 1);
         m_CurrentMembank = new_bank;
      }
      m_AddressOffset = address - m_CurrentMembank->base_guest;
      m_HexWidget->setOffset(address);

      m_Slider->setMinimum(0);
      m_Slider->setMaximum(m_CurrentMembank->size);
      m_Slider->setValue(address - m_CurrentMembank->base_guest);
   }
}

void CleMemoryInspector::onChangeCompareType()
{
   switch (getCurrentCompareType())
   {
   case CLE_CMPTYPE_EQUAL:
   case CLE_CMPTYPE_GREATER:
   case CLE_CMPTYPE_LESS:
   case CLE_CMPTYPE_NOT_EQUAL:
      m_TextEntry->setPlaceholderText(tr("previous value"));
      break;
   case CLE_CMPTYPE_INCREASED:
   case CLE_CMPTYPE_DECREASED:
      m_TextEntry->setPlaceholderText(tr("any amount"));
      break;
   case CLE_CMPTYPE_ABOVE:
   case CLE_CMPTYPE_BELOW:
   default:
      m_TextEntry->setPlaceholderText("");
   }
   m_CurrentSearch->setCompareType(getCurrentCompareType());
}

void CleMemoryInspector::onChangeScrollbar(int value)
{
   onAddressChanged(m_CurrentMembank->base_guest + value);
}

void CleMemoryInspector::onChangeSizeType()
{
   m_CurrentSearch->setValueType(getCurrentSizeType());
   m_HexWidget->setSize(cl_sizeof_memtype(getCurrentSizeType()));
}

void CleMemoryInspector::onChangeTab()
{
   uint8_t new_tab = m_Tabs->currentIndex();

   /* Was the "new tab" icon clicked? */
   if (new_tab >= m_TabCount)
   {
      /* Setup this tab to be a new search, add a "+" button */
      m_Tabs->setTabText(new_tab, tr("New Search"));
      m_Tabs->addTab("+");
      m_TabCount++;
      m_Searches[new_tab] = new CleResultTableNormal(this);
      m_TableStack->addWidget(m_Searches[new_tab]->getTable());
   }
   m_CurrentSearch = m_Searches[new_tab];
   m_CurrentSearch->rebuild();
   m_TableStack->setCurrentIndex(new_tab);

   m_CompareDropdown->setCurrentIndex(
      m_CompareDropdown->findData(m_CurrentSearch->getCompareType()));
   m_SizeDropdown->setCurrentIndex(
      m_SizeDropdown->findData(m_CurrentSearch->getValueType()));
   m_HexWidget->setSize(cl_sizeof_memtype(getCurrentSizeType()));
}

void CleMemoryInspector::onClickNew()
{
   if (!memory.region_count)
      return;
   else
   {
      m_TableStack->removeWidget(m_Searches[m_Tabs->currentIndex()]->getTable());
      delete m_Searches[m_Tabs->currentIndex()];
      m_Searches[m_Tabs->currentIndex()] = new CleResultTableNormal(this);
      m_CurrentSearch = m_Searches[m_Tabs->currentIndex()];

      m_TableStack->insertWidget(m_Tabs->currentIndex(), m_CurrentSearch->getTable());
      m_TableStack->setCurrentWidget(m_CurrentSearch->getTable());
      m_Tabs->setTabTextColor(m_Tabs->currentIndex(), Qt::white);

      m_CurrentSearch->setCompareType(getCurrentCompareType());
      m_CurrentSearch->setValueType(getCurrentSizeType());
   }
}

void CleMemoryInspector::onClickSearch()
{
   if (!memory.region_count)
      return;
   else
   {
      if (!m_CurrentSearch->isInitted())
         onClickNew();
      if (!m_CurrentSearch->step(m_TextEntry->text()))
      {
         cl_log("Search input failed: %s\n", m_TextEntry->text().toStdString().c_str());
         m_TextEntry->setText("");
      }
   }
}

void CleMemoryInspector::onHexWidgetValueEdited(cl_addr_t address, uint8_t value)
{
  cl_write_memory(NULL, address, cl_sizeof_memtype(getCurrentSizeType()), &value);
}

void CleMemoryInspector::onClickTabRename()
{
   QString text = QInputDialog::getText
   (
      this, 
      tr("Rename"), 
      tr("Search tab name:")
   );

   if (m_ClickedTab >= 0 && m_ClickedTab < m_TabCount && !text.isEmpty())
      m_Tabs->setTabText(m_ClickedTab, text);
}

void CleMemoryInspector::onRightClickTabs(const QPoint &pos)
{
   if (pos.isNull())
      return;
   else
   {
      m_ClickedTab = m_Tabs->tabAt(pos);
      if (m_ClickedTab < 0 || m_ClickedTab >= m_TabCount)
         return;
      else
      {
         QMenu menu;
         QAction *action_rename = menu.addAction(tr("Rename"));

         connect(action_rename, SIGNAL(triggered()), this, 
            SLOT(onClickTabRename()));

         menu.exec(m_Tabs->mapToGlobal(pos));
      }
   }
}

void CleMemoryInspector::requestAddMemoryNote(cl_memnote_t note)
{
   if (!session.game_id)
   {
      QMessageBox::warning(this, "Live Editor", 
         tr("The content you are running was not recognized by the server, so "
            "you cannot submit memory notes.")
      );
   }
   else
   {
      m_MemoryNoteSubmit = new CleMemoryNoteSubmit(note);
      m_MemoryNoteSubmit->show();
   }
}

void CleMemoryInspector::requestAddMemoryNote(cl_addr_t address)
{
   cl_memnote_t note;

   note.address         = address;
   note.type            = getCurrentSizeType();
   note.pointer_offsets = NULL;
   note.pointer_passes  = 0;

   requestAddMemoryNote(note);
}

void CleMemoryInspector::requestPointerSearch(cl_addr_t address)
{
   if (!address)
      return;
   else
   {
      m_Searches[m_TabCount] = new CleResultTablePointer
      (
         this,
         address, 
         getCurrentSizeType(),
         3,
         0x10000,
         100000
      );

      m_TabCount++;
      m_TableStack->addWidget(m_Searches[m_TabCount-1]->getTable());
      m_Tabs->setCurrentIndex(m_TabCount - 1);
      m_Tabs->setTabText(m_Tabs->currentIndex(), tr("Pointers"));
      m_Tabs->setTabTextColor(m_Tabs->currentIndex(), Qt::yellow);
      m_Tabs->addTab("+");
      m_CurrentSearch = m_Searches[m_TabCount-1];
   }
}

void CleMemoryInspector::run()
{
   m_CurrentSearch->run();
   cl_read_memory(m_BufferCurrent, nullptr, m_AddressOffset + m_CurrentMembank->base_guest, 256);
   m_HexWidget->refresh(m_BufferCurrent, m_BufferPrevious);
   memcpy(m_BufferPrevious, m_BufferCurrent, 256);
}
