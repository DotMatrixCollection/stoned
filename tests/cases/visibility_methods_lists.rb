class VisibilityBox
  def pub; end
  protected def prot; end
  private def priv; end
end

obj = VisibilityBox.new
p obj.public_methods.include?(:pub)
p obj.protected_methods.include?(:prot)
p obj.private_methods.include?(:priv)
