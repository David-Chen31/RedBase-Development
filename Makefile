# RedBase Database System Makefile

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++14 -Wall -I PF/include -I PF/internal -I RM/include -I IX/include -I IX/internal -I SM/include -I QL/include -I QL/internal

# 目标文件目录
OBJDIR = obj
$(shell mkdir -p $(OBJDIR))

# 源文件
PF_SOURCES = PF/src/pf_manager.cc PF/src/pf_filehandle.cc PF/src/pf_pagehandle.cc PF/src/pf_statistics.cc PF/internal/buffer_manager.cc PF/internal/hash_table.cc PF/src/pf_error.cc
RM_SOURCES = RM/src/rm_manager.cc RM/src/rm_filehandle.cc RM/src/rm_filescan.cc RM/src/rm_record.cc RM/src/rm_rid.cc RM/src/rm_error.cc RM/src/rm_internal.cc
IX_SOURCES = IX/src/ix_manager.cc IX/src/ix_indexhandle.cc IX/src/ix_indexscan.cc IX/src/ix_btree.cc IX/src/ix_error.cc
SM_SOURCES = SM/src/sm_manager.cc SM/src/sm_manager2.cc SM/src/sm_catalog.cc SM/src/sm_printer.cc SM/src/sm_error.cc SM/src/sm_internal.cc
QL_SOURCES = QL/src/ql_manager.cc QL/src/ql_plannode.cc QL/src/ql_optimizer.cc QL/src/ql_error.cc

# 主程序源文件
MAIN_SOURCES = redbase_main.cc QL/src/sql_parser.cc

# 目标文件
PF_OBJECTS = $(PF_SOURCES:%.cc=$(OBJDIR)/%.o)
RM_OBJECTS = $(RM_SOURCES:%.cc=$(OBJDIR)/%.o)
IX_OBJECTS = $(IX_SOURCES:%.cc=$(OBJDIR)/%.o)
SM_OBJECTS = $(SM_SOURCES:%.cc=$(OBJDIR)/%.o)
QL_OBJECTS = $(QL_SOURCES:%.cc=$(OBJDIR)/%.o)
MAIN_OBJECTS = $(MAIN_SOURCES:%.cc=$(OBJDIR)/%.o)

ALL_OBJECTS = $(PF_OBJECTS) $(RM_OBJECTS) $(IX_OBJECTS) $(SM_OBJECTS) $(QL_OBJECTS) $(MAIN_OBJECTS)

# 主目标
redbase: $(ALL_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 编译规则
$(OBJDIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理规则
clean:
	rm -rf $(OBJDIR) redbase redbase.exe

# 安装规则
install: redbase
	cp redbase /usr/local/bin/

.PHONY: clean install

# 调试信息
debug:
	@echo "PF Objects: $(PF_OBJECTS)"
	@echo "RM Objects: $(RM_OBJECTS)"
	@echo "IX Objects: $(IX_OBJECTS)"
	@echo "SM Objects: $(SM_OBJECTS)"
	@echo "QL Objects: $(QL_OBJECTS)"
	@echo "Main Objects: $(MAIN_OBJECTS)"