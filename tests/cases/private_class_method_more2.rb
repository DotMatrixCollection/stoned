class PrivateClassMethodMore
  def self.hidden; :hidden; end
  private_class_method :hidden
end

p PrivateClassMethodMore.respond_to?(:hidden)
p PrivateClassMethodMore.respond_to?(:hidden, true)
