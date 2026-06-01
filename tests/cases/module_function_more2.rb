module MFMore
  def helper; :instance; end
  module_function :helper
end

p MFMore.helper
p MFMore.private_instance_methods.include?(:helper)
