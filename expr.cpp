#include <iostream>
#include <string>
#include <memory>
#include <z3++.h>

class VarExpr;
class LessThanExpr;
class AndExpr;
class OrExpr;

class ExprVisitor {
public:
    virtual void visit(const VarExpr& expr) = 0;
    virtual void visit(const LessThanExpr& expr) = 0;
    virtual void visit(const AndExpr& expr) = 0;
    virtual void visit(const OrExpr& expr) = 0;
};

class Expr {
public:
    virtual void accept(ExprVisitor& visitor) const = 0;
    virtual void print() const = 0;
};

class VarExpr : public Expr {
private:
    std::string name;
public:
    VarExpr(const std::string& name_) : name(name_) {}

    const std::string& getName() const {
        return name;
    }
    
    void print() const override {
        std::cout << name;
    }
    void accept(ExprVisitor& visitor) const override;
};

class BinExpr : public Expr {
private:
    std::shared_ptr<Expr> lhs;
    std::shared_ptr<Expr> rhs;
public:
    BinExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_) : lhs(lhs_), rhs(rhs_) {}

    const std::shared_ptr<Expr>& getLhs() const {
        return lhs;
    }

    const std::shared_ptr<Expr>& getRhs() const {
        return rhs;
    }
};

class LessThanExpr : public BinExpr {
public:
    LessThanExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_) : BinExpr(lhs_, rhs_) {}

    void accept(ExprVisitor& visitor) const override;
    
    void print() const override {
        std::cout << "(";
        getLhs()->print();
        std::cout << " < ";
        getRhs()->print();
        std::cout << ")";
    }
};

class AndExpr : public BinExpr {
public:
    AndExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_) : BinExpr(lhs_, rhs_) {}

    void accept(ExprVisitor& visitor) const override;
    
    void print() const override {
        std::cout << "(";
        getLhs()->print();
        std::cout << " && ";
        getRhs()->print();
        std::cout << ")";
    }
};

class OrExpr : public BinExpr {
public:
    OrExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_) : BinExpr(lhs_, rhs_) {}

    void accept(ExprVisitor& visitor) const override;
    
    void print() const override {
        std::cout << "(";
        getLhs()->print();
        std::cout << " || ";
        getRhs()->print();
        std::cout << ")";
    }
};

void VarExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void LessThanExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void AndExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void OrExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }

class ExprBuilder {
public:
    static std::shared_ptr<Expr> var(const std::string& name) {
        return std::make_shared<VarExpr>(name);
    }

    static std::shared_ptr<Expr> lessThan(const std::shared_ptr<Expr>& left, const std::shared_ptr<Expr>& right) {
        return std::make_shared<LessThanExpr>(left, right);
    }

    static std::shared_ptr<Expr> andExpr(const std::shared_ptr<Expr>& left, const std::shared_ptr<Expr>& right) {
        return std::make_shared<AndExpr>(left, right);
    }

    static std::shared_ptr<Expr> orExpr(const std::shared_ptr<Expr>& left, const std::shared_ptr<Expr>& right) {
        return std::make_shared<OrExpr>(left, right);
    }
};

class Z3ExprVisitor : public ExprVisitor {
private:
    z3::context& ctx;
    z3::expr result;
public:
    Z3ExprVisitor(z3::context& ctx_) : ctx(ctx_), result(ctx_) {}

    void visit(const VarExpr& expr) override {
        result = ctx.int_const(expr.getName().c_str());
    }

    void visit(const LessThanExpr& expr) override {
        expr.getLhs()->accept(*this);
        z3::expr lhs = result;
        expr.getRhs()->accept(*this);
        z3::expr rhs = result;
        result = lhs < rhs;
    }

    void visit(const AndExpr& expr) override {
        expr.getLhs()->accept(*this);
        z3::expr lhs = result;
        expr.getRhs()->accept(*this);
        z3::expr rhs = result;
        result = lhs && rhs;
    }

    void visit(const OrExpr& expr) override {
        expr.getLhs()->accept(*this);
        z3::expr lhs = result;
        expr.getRhs()->accept(*this);
        z3::expr rhs = result;
        result = lhs || rhs;
    }

    z3::expr getResult() const {
        return result;
    }
};
