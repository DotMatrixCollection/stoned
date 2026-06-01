class FreezeMore; end
obj = FreezeMore.new
p obj.frozen?
p obj.freeze.equal?(obj)
p obj.frozen?
