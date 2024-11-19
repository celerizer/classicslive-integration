#ifndef CLE_RESULT_TABLE_POINTER_H
#define CLE_RESULT_TABLE_POINTER_H

#include "cle_result_table.h"

class CleResultTablePointer : public CleResultTable
{
  Q_OBJECT

public:
  CleResultTablePointer(QWidget *parent, uint32_t address, uint8_t size,
                        uint8_t passes, uint32_t range, uint32_t max_results);
  ~CleResultTablePointer() override;

  cl_addr_t getClickedResultAddress() override;
  void *getSearchData() override;
  bool isInitted() override { return true; }
  void rebuild() override;
  void reset(void) override;
  void run() override;
  bool step(const QString& text) override;

  uint8_t getCompareType() override { return m_Search.params.compare_type; }
  uint8_t getValueType() override { return m_Search.params.value_type; }

  void setCompareType(const uint8_t new_type) override
  {
    m_Search.params.compare_type = new_type;
  }
  void setValueType(const uint8_t new_type) override
  {
    m_Search.params.value_type = new_type;
    m_Search.params.size = cl_sizeof_memtype(new_type);
  }

public slots:
  void onClickResultAddMemoryNote();
  void onResultClick(QTableWidgetItem *item) override;
  void onResultDoubleClick(void) override;
  void onResultEdited(QTableWidgetItem *item) override;
  void onResultRightClick(const QPoint&) override;

signals:
  void addressChanged(cl_addr_t address);
  void requestAddMemoryNote(cl_memnote_t note);
  void requestRemove(uint32_t index);

private:
  uint8_t m_ColAddress;
  uint8_t m_ColValuePrev;
  uint8_t m_ColValueCurr;
  cl_pointersearch_t m_Search;
};

#endif
